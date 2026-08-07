#include "SdlAudioSink.h"

#include <SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include "../events/NotificationBus.h"

namespace {

bool isSupportedSourceFormat(const AudioFormat& fmt) {
    return fmt.sampleRate > 0 && (fmt.channels == 1 || fmt.channels == 2);
}

} // namespace

SdlAudioSink::SdlAudioSink(NotificationBus* bus) : bus_(bus) {}

SdlAudioSink::~SdlAudioSink() {
    close();
}

bool SdlAudioSink::open(const AudioFormat& fmt, FrameProvider provider, const std::string& deviceName, const int desiredBufferFrames) {
    close();
    if (!isSupportedSourceFormat(fmt)) {
        if (bus_) bus_->push("Audio: invalid format", NotifyLevel::Error);
        return false;
    }
    if (!provider) {
        if (bus_) bus_->push("Audio: provider is null", NotifyLevel::Error);
        return false;
    }

    if (!audio_.ok()) {
        if (bus_) bus_->push(std::string("SDL init failed: ") + SDL_GetError(), NotifyLevel::Error);
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = fmt.sampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = static_cast<Uint8>(fmt.channels);
    desired.samples = static_cast<Uint16>(desiredBufferFrames);
    desired.callback = &SdlAudioSink::sdlCallback;
    desired.userdata = this;

    const char* sdlDeviceName = deviceName.empty() ? nullptr : deviceName.c_str();

    // Let the device pick its own rate and channel count. Whatever it settles
    // on becomes fixed for the lifetime of the device, and sources that do not
    // match are resampled into it instead of forcing the device to reopen.
    SDL_AudioSpec obtained{};
    const SDL_AudioDeviceID dev = SDL_OpenAudioDevice(
        sdlDeviceName, 0, &desired, &obtained,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        if (bus_) bus_->push(std::string("Audio device failed: ") + SDL_GetError(), NotifyLevel::Error);
        return false;
    }

    if (obtained.format != AUDIO_S16SYS) {
        if (bus_) bus_->push("Audio: unsupported format", NotifyLevel::Error);
        SDL_CloseAudioDevice(dev);
        return false;
    }
    if (obtained.freq <= 0 || obtained.channels == 0) {
        if (bus_) bus_->push("Audio: format mismatch", NotifyLevel::Error);
        SDL_CloseAudioDevice(dev);
        return false;
    }

    device_ = static_cast<std::uintptr_t>(dev);
    srcFmt_ = fmt;
    deviceFmt_ = AudioFormat{obtained.freq, static_cast<int>(obtained.channels)};
    provider_ = std::move(provider);
    deviceBufferBytes_ = static_cast<int>(obtained.size);
    mixBuffer_.assign(obtained.size, 0);  // preallocate so fill() never allocates
    open_ = true;

    if (!rebuildConverter()) {
        close();
        return false;
    }
    return true;
}

bool SdlAudioSink::setSourceFormat(const AudioFormat& fmt) {
    if (!open_) return false;
    if (!isSupportedSourceFormat(fmt)) {
        if (bus_) bus_->push("Audio: invalid format", NotifyLevel::Error);
        return false;
    }

    // The device keeps running; only the conversion in front of it is rebuilt.
    // Locking is what makes the swap safe against an in-flight callback.
    SDL_LockAudioDevice(static_cast<SDL_AudioDeviceID>(device_));
    srcFmt_ = fmt;
    const bool ok = rebuildConverter();
    SDL_UnlockAudioDevice(static_cast<SDL_AudioDeviceID>(device_));

    if (!ok && bus_) {
        bus_->push("Audio: cannot convert to device format", NotifyLevel::Error);
    }
    return ok;
}

bool SdlAudioSink::rebuildConverter() {
    if (converter_) {
        SDL_FreeAudioStream(reinterpret_cast<SDL_AudioStream*>(converter_));
        converter_ = nullptr;
    }

    const std::size_t deviceFrames = deviceFmt_.channels > 0
        ? static_cast<std::size_t>(deviceBufferBytes_) / (sizeof(int16_t) * static_cast<std::size_t>(deviceFmt_.channels))
        : 0;

    if (srcFmt_ == deviceFmt_) {
        // Fast path: provider writes straight into the device buffer.
        srcFramesPerPull_ = deviceFrames;
        srcBuffer_.clear();
        srcBuffer_.shrink_to_fit();
        return true;
    }

    converter_ = reinterpret_cast<_SDL_AudioStream*>(SDL_NewAudioStream(
        AUDIO_S16SYS, static_cast<Uint8>(srcFmt_.channels), srcFmt_.sampleRate,
        AUDIO_S16SYS, static_cast<Uint8>(deviceFmt_.channels), deviceFmt_.sampleRate));
    if (!converter_) {
        return false;
    }

    // One device buffer's worth of source audio, plus a frame of slack for the
    // rounding in the rate ratio, so a single pull can always satisfy a callback.
    const std::size_t framesPerPull = static_cast<std::size_t>(
        (static_cast<std::uint64_t>(deviceFrames) * static_cast<std::uint64_t>(srcFmt_.sampleRate)
         + static_cast<std::uint64_t>(deviceFmt_.sampleRate) - 1)
        / static_cast<std::uint64_t>(deviceFmt_.sampleRate)) + 1;

    srcFramesPerPull_ = framesPerPull;
    srcBuffer_.assign(framesPerPull * static_cast<std::size_t>(srcFmt_.channels), 0);
    return true;
}

void SdlAudioSink::start() {
    if (!open_) return;
    SDL_PauseAudioDevice(static_cast<SDL_AudioDeviceID>(device_), 0);
}

void SdlAudioSink::stop() {
    if (!open_) return;
    SDL_PauseAudioDevice(static_cast<SDL_AudioDeviceID>(device_), 1);
}

void SdlAudioSink::close() {
    if (!open_) return;
    SDL_CloseAudioDevice(static_cast<SDL_AudioDeviceID>(device_));
    if (converter_) {
        SDL_FreeAudioStream(reinterpret_cast<SDL_AudioStream*>(converter_));
        converter_ = nullptr;
    }
    device_ = 0;
    provider_ = nullptr;
    srcFmt_ = AudioFormat{};
    deviceFmt_ = AudioFormat{};
    deviceBufferBytes_ = 0;
    srcFramesPerPull_ = 0;
    open_ = false;
}

bool SdlAudioSink::isOpen() const {
    return open_;
}

void SdlAudioSink::setVolume(int percent) {
    volume_.store(std::clamp(percent, 0, 100), std::memory_order_relaxed);
}

int SdlAudioSink::getVolume() const {
    return volume_.load(std::memory_order_relaxed);
}

std::vector<std::string> SdlAudioSink::listOutputDevices() {
    SdlAudioSubsystem audio;
    if (!audio.ok()) {
        return {};
    }

    // Collect unique device names (ALSA often reports the same chipset name
    // for multiple HDMI/DP outputs — de-duplicate to avoid confusing listings)
    std::vector<std::string> devices;
    const int count = SDL_GetNumAudioDevices(0); // 0 = output devices
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, 0);
        if (!name) continue;
        std::string nameStr(name);
        bool duplicate = false;
        for (const auto& existing : devices) {
            if (existing == nameStr) { duplicate = true; break; }
        }
        if (!duplicate) {
            devices.push_back(std::move(nameStr));
        }
    }
    return devices;
}

void SdlAudioSink::sdlCallback(void* userdata, std::uint8_t* stream, const int len) {
    static_cast<SdlAudioSink*>(userdata)->fill(stream, len);
}

void SdlAudioSink::fill(std::uint8_t* stream, const int len) {
    std::size_t bytesWritten = 0;

    if (!converter_) {
        const std::size_t bytesPerFrame = sizeof(int16_t) * static_cast<std::size_t>(deviceFmt_.channels);
        const std::size_t framesRequested = bytesPerFrame > 0
            ? static_cast<std::size_t>(len) / bytesPerFrame
            : 0;

        auto* out = reinterpret_cast<int16_t*>(stream);
        const std::size_t framesWritten = provider_ ? provider_(out, framesRequested) : 0;
        bytesWritten = framesWritten * bytesPerFrame;
    } else {
        auto* converter = reinterpret_cast<SDL_AudioStream*>(converter_);
        const std::size_t srcBytesPerFrame = sizeof(int16_t) * static_cast<std::size_t>(srcFmt_.channels);

        while (SDL_AudioStreamAvailable(converter) < len) {
            const std::size_t framesRead = provider_ ? provider_(srcBuffer_.data(), srcFramesPerPull_) : 0;
            if (framesRead == 0) {
                break; // starved: the tail of the callback pads with silence
            }
            if (SDL_AudioStreamPut(converter, srcBuffer_.data(),
                                   static_cast<int>(framesRead * srcBytesPerFrame)) < 0) {
                break;
            }
        }

        const int got = SDL_AudioStreamGet(converter, stream, len);
        bytesWritten = got > 0 ? static_cast<std::size_t>(got) : 0;
    }

    if (bytesWritten < static_cast<std::size_t>(len)) {
        std::memset(stream + bytesWritten, 0, static_cast<std::size_t>(len) - bytesWritten);
    }

    // Apply volume scaling
    int vol = volume_.load(std::memory_order_relaxed);
    if (vol < 100) {
        // SDL_MIX_MAXVOLUME is 128; scale our 0-100 range
        int sdlVol = vol * SDL_MIX_MAXVOLUME / 100;
        // Copy to pre-allocated buffer, then mix scaled audio back
        // SDL_MixAudioFormat adds to dst, so we clear stream first
        const auto byteLen = static_cast<std::size_t>(len);
        if (mixBuffer_.size() < byteLen) mixBuffer_.resize(byteLen);
        std::memcpy(mixBuffer_.data(), stream, byteLen);
        std::memset(stream, 0, byteLen);
        SDL_MixAudioFormat(stream, mixBuffer_.data(), AUDIO_S16SYS, static_cast<Uint32>(len), sdlVol);
    }
}
