#include "PlaybackEngine.h"
#include "../decode/AudioDecoderFactory.h"
#include "../decode/IAudioDecoder.h"
#include "../audio/SdlAudioSink.h"
#include "../util/RingBuffer.h"
#include "../config/Config.h"
#include "../events/NotificationBus.h"
#include "DecodeThread.h"
#include <algorithm>
constexpr int SPECTRUM_BARS = 40;
PlaybackEngine::PlaybackEngine(NotificationBus* bus,
                               DecoderFactory decoderFactory,
                               SinkFactory sinkFactory)
    : decoderFactory_(decoderFactory
        ? std::move(decoderFactory)
        : [](const std::filesystem::path& p) { return AudioDecoderFactory::create(p); })
    , sinkFactory_(sinkFactory
        ? std::move(sinkFactory)
        : [](NotificationBus* b) { return std::make_unique<SdlAudioSink>(b); })
    , state_(State::Stopped)
    , sampleRate_(0)
    , channels_(0)
    , silentCallbacks_(0)
    , spectrumAnalyzer_(SPECTRUM_BARS)
    , samplesPlayed_(0)
    , totalSamples_(0)
    , bus_(bus)
{
}

PlaybackEngine::~PlaybackEngine() {
    stop();
}

bool PlaybackEngine::load(const std::filesystem::path& audioFile) {
    // Tears down the source only. The sink stays open across tracks: a track
    // change is a change of what we feed the device, not of the device itself.
    stopPlayback();

    decoder_ = decoderFactory_(audioFile);
    if (!decoder_) {
        if (bus_) bus_->push("Unsupported format: " + audioFile.filename().string(), NotifyLevel::Error);
        return false;
    }
    if (!decoder_->open(audioFile)) {
        if (bus_) bus_->push("Failed to open: " + audioFile.filename().string(), NotifyLevel::Error);
        decoder_.reset();
        return false;
    }

    // Store file info
    currentFile_ = audioFile;
    sampleRate_ = decoder_->sampleRate();
    channels_ = decoder_->channels();
    totalSamples_ = decoder_->totalSamples();
    samplesPlayed_.store(0, std::memory_order_release);
    bpmDetector_.reset();

    // Sized for the worst case once, so a track change never reallocates it and
    // the provider's pointer to it stays valid for the life of the engine.
    if (!ringBuffer_) {
        ringBuffer_ = std::make_unique<RingBuffer<int16_t>>(
            Config::RING_BUFFER_SIZE_SECONDS * Config::MAX_SAMPLE_RATE * Config::MAX_CHANNELS);
    }
    ringBuffer_->clear(); // safe: decode thread stopped and device paused

    decodeThread_ = std::make_unique<DecodeThread>();

    AudioFormat fmt;
    fmt.sampleRate = sampleRate_;
    fmt.channels = channels_;

    if (!prepareSink(fmt)) {
        decoder_->close();
        decoder_.reset();
        decodeThread_.reset();
        return false;
    }

    sink_->setVolume(volume_.load(std::memory_order_relaxed));
    state_.store(State::Stopped, std::memory_order_release);
    return true;
}

bool PlaybackEngine::prepareSink(const AudioFormat& fmt) {
    // Already have a working device: just re-point it. No close, no reopen, so
    // nothing here can lose a device that takes time to re-acquire (bluetooth).
    if (sink_ && sink_->isOpen() && sink_->setSourceFormat(fmt)) {
        return true;
    }

    if (openSink(fmt)) {
        return true;
    }

    if (bus_) bus_->push("Failed to open audio sink", NotifyLevel::Error);
    sink_.reset();
    return false;
}

bool PlaybackEngine::openSink(const AudioFormat& fmt) {
    sink_ = sinkFactory_(bus_);
    if (!sink_) return false;

    if (sink_->open(fmt, makeAudioProvider(), outputDeviceName_)) {
        return true;
    }

    // The selected device disappeared (unpaired bluetooth, hot-unplug).
    // Fall back to the system default instead of killing playback.
    if (outputDeviceName_.empty() || !sink_->open(fmt, makeAudioProvider(), "")) {
        return false;
    }

    if (bus_) bus_->push("Output '" + deviceLabel(outputDeviceName_)
                         + "' unavailable, using system default", NotifyLevel::Error);
    outputDeviceName_.clear();
    return true;
}

void PlaybackEngine::play() {
    const State currentState = state_.load(std::memory_order_acquire);

    if (currentState == State::Playing) {
        return;
    }

    if (!decoder_ || !sink_) {
        if (bus_) bus_->push("No file loaded", NotifyLevel::Error);
        return;
    }

    if (currentState == State::Stopped) {
        if (!startPlayback()) {
            if (bus_) bus_->push("Failed to start playback", NotifyLevel::Error);
            return;
        }
    } else if (currentState == State::Paused) {
        sink_->start();
    }

    state_.store(State::Playing, std::memory_order_release);
}

void PlaybackEngine::pause() {
    if (state_.load(std::memory_order_acquire) != State::Playing) {
        return;
    }

    if (sink_) {
        sink_->stop();
    }

    state_.store(State::Paused, std::memory_order_release);
}

void PlaybackEngine::stop() {
    stopPlayback();
}

bool PlaybackEngine::isPlaying() const {
    return state_.load(std::memory_order_acquire) == State::Playing;
}

bool PlaybackEngine::isPaused() const {
    return state_.load(std::memory_order_acquire) == State::Paused;
}

bool PlaybackEngine::isStopped() const {
    return state_.load(std::memory_order_acquire) == State::Stopped;
}

PlaybackEngine::State PlaybackEngine::getState() const {
    return state_.load(std::memory_order_acquire);
}

bool PlaybackEngine::hasReachedEndOfStream() const {
    return silentCallbacks_.load(std::memory_order_acquire) >= Config::END_OF_STREAM_THRESHOLD;
}

std::filesystem::path PlaybackEngine::getCurrentFile() const {
    return currentFile_;
}

int PlaybackEngine::getSampleRate() const {
    return sampleRate_;
}

int PlaybackEngine::getChannels() const {
    return channels_;
}

float PlaybackEngine::getBpmLoadFactor() const {
    return bpmDetector_.getLoadFactor();
}

float PlaybackEngine::getPlaybackProgress() const {
    if (totalSamples_ == 0) return 0.0f;
    auto played = samplesPlayed_.load(std::memory_order_relaxed);
    float progress = static_cast<float>(played) / static_cast<float>(totalSamples_);
    return std::min(progress, 1.0f);
}

bool PlaybackEngine::startPlayback() {

    if (!decoder_ || !ringBuffer_ || !decodeThread_ || !sink_) {
        return false;
    }

    silentCallbacks_.store(0, std::memory_order_release);

    decodeThread_->start(decoder_.get(), ringBuffer_.get(), Config::DECODE_CHUNK_FRAMES);

    const std::size_t targetSamples = static_cast<std::size_t>(
        ringBuffer_->availableToWrite() * Config::RING_BUFFER_PREFILL_PERCENT
    );
    while (ringBuffer_->availableToRead() < targetSamples) {
        if (decodeThread_->isEndOfStream()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    sink_->start();

    return true;
}

void PlaybackEngine::setVolume(int percent) {
    volume_.store(std::clamp(percent, 0, 100), std::memory_order_relaxed);
    if (sink_) sink_->setVolume(percent);
}

int PlaybackEngine::getVolume() const {
    return volume_.load(std::memory_order_relaxed);
}

PlaybackEngine::FrameProvider PlaybackEngine::makeAudioProvider() {
    return [this](int16_t* dst, std::size_t framesRequested) -> std::size_t {
        const std::size_t samplesRequested = framesRequested * channels_;
        const std::size_t samplesRead = ringBuffer_ ? ringBuffer_->read(dst, samplesRequested) : 0;
        const std::size_t framesRead = samplesRead / channels_;

        if (framesRead == 0) {
            silentCallbacks_.fetch_add(1, std::memory_order_relaxed);
        } else {
            silentCallbacks_.store(0, std::memory_order_relaxed);
            samplesPlayed_.fetch_add(samplesRead, std::memory_order_relaxed);
            spectrumAnalyzer_.feed(dst, samplesRead, channels_);
            bpmDetector_.feed(dst, samplesRead, channels_, sampleRate_);
        }

        return framesRead;
    };
}

std::string PlaybackEngine::deviceLabel(const std::string& name) const {
    return name.empty() ? std::string("system default") : name;
}

bool PlaybackEngine::setOutputDevice(const std::string& deviceName) {
    if (deviceName == outputDeviceName_) {
        return true;
    }

    // Nothing loaded: probe the device so a broken selection is rejected now
    // rather than at the next load().
    if (!sink_) {
        AudioFormat probeFmt;
        probeFmt.sampleRate = sampleRate_ > 0 ? sampleRate_.load(std::memory_order_relaxed) : 44100;
        probeFmt.channels = channels_ > 0 ? channels_.load(std::memory_order_relaxed) : 2;

        auto probe = sinkFactory_(bus_);
        const bool usable = probe && probe->open(
            probeFmt,
            [](int16_t*, std::size_t) -> std::size_t { return 0; },
            deviceName);
        if (!usable) {
            if (bus_) bus_->push("Output '" + deviceLabel(deviceName) + "' unavailable, keeping "
                                 + deviceLabel(outputDeviceName_), NotifyLevel::Error);
            return false;
        }
        probe->close();
        outputDeviceName_ = deviceName;
        if (bus_) bus_->push("Output: " + deviceLabel(deviceName));
        return true;
    }

    // Live swap: open the new device before touching the running one, so a
    // failure leaves the current sink untouched and playback uninterrupted.
    AudioFormat fmt;
    fmt.sampleRate = sampleRate_.load(std::memory_order_relaxed);
    fmt.channels = channels_.load(std::memory_order_relaxed);

    auto newSink = sinkFactory_(bus_);
    if (!newSink || !newSink->open(fmt, makeAudioProvider(), deviceName)) {
        if (bus_) bus_->push("Output '" + deviceLabel(deviceName) + "' unavailable, keeping "
                             + deviceLabel(outputDeviceName_), NotifyLevel::Error);
        return false;
    }

    newSink->setVolume(volume_.load(std::memory_order_relaxed));

    const bool wasPlaying = state_.load(std::memory_order_acquire) == State::Playing;
    sink_->stop();
    sink_->close();
    sink_ = std::move(newSink);
    if (wasPlaying) {
        sink_->start();
    }

    outputDeviceName_ = deviceName;
    if (bus_) bus_->push("Output: " + deviceLabel(deviceName));
    return true;
}

std::string PlaybackEngine::getOutputDevice() const {
    return outputDeviceName_;
}

std::vector<std::string> PlaybackEngine::listOutputDevices() {
    return SdlAudioSink::listOutputDevices();
}

void PlaybackEngine::stopPlayback() {
    // Must be called with mutex_ held

    // Paused, not closed. Closing here is what used to force every track change
    // to re-acquire the output device. The device is released in the destructor.
    if (sink_) {
        sink_->stop();
    }

    if (decodeThread_) {
        decodeThread_->stop();
        decodeThread_.reset();
    }
    if (decoder_) {
        decoder_->close();
        decoder_.reset();
    }
    if (ringBuffer_) {
        ringBuffer_->clear(); // kept allocated, see load()
    }

    state_.store(State::Stopped, std::memory_order_release);
    currentFile_.clear();
    totalSamples_ = 0;
    samplesPlayed_.store(0, std::memory_order_release);
    // sampleRate_/channels_ deliberately survive: they describe the format the
    // still-open sink is configured for, which setOutputDevice() needs.
}
