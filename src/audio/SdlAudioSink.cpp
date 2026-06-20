#include "SdlAudioSink.h"

#include <SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include "../events/NotificationBus.h"

SdlAudioSink::SdlAudioSink(NotificationBus* bus) : bus_(bus) {}

SdlAudioSink::~SdlAudioSink() {
    close();
}

bool SdlAudioSink::open(const AudioFormat& fmt, FrameProvider provider, const std::string& deviceName, const int desiredBufferFrames) {
    close();
    if (fmt.sampleRate <= 0 || (fmt.channels != 1 && fmt.channels != 2)) {
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

    SDL_AudioSpec obtained{};
    const SDL_AudioDeviceID dev = SDL_OpenAudioDevice(sdlDeviceName, 0, &desired, &obtained, 0);
    if (dev == 0) {
        if (bus_) bus_->push(std::string("Audio device failed: ") + SDL_GetError(), NotifyLevel::Error);
        return false;
    }

    if (obtained.format != AUDIO_S16SYS) {
        if (bus_) bus_->push("Audio: unsupported format", NotifyLevel::Error);
        SDL_CloseAudioDevice(dev);
        return false;
    }

    if (obtained.freq != desired.freq || obtained.channels != desired.channels) {
        if (bus_) bus_->push("Audio: format mismatch", NotifyLevel::Error);
        SDL_CloseAudioDevice(dev);
        return false;
    }

    fmt_ = fmt;
    provider_ = std::move(provider);
    device_ = static_cast<std::uintptr_t>(dev);
    mixBuffer_.assign(obtained.size, 0);  // preallocate so fill() never allocates
    open_ = true;
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
    device_ = 0;
    provider_ = nullptr;
    fmt_ = AudioFormat{};
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
    const std::size_t bytesPerFrame = sizeof(int16_t) * static_cast<std::size_t>(fmt_.channels);
    const std::size_t framesRequested = static_cast<std::size_t>(len) / bytesPerFrame;

    auto* out = reinterpret_cast<int16_t*>(stream);
    const std::size_t samplesRequested = framesRequested * static_cast<std::size_t>(fmt_.channels);

    const std::size_t framesWritten = provider_ ? provider_(out, framesRequested) : 0;
    const std::size_t samplesWritten = framesWritten * static_cast<std::size_t>(fmt_.channels);

    if (samplesWritten < samplesRequested) {
        std::memset(out + samplesWritten, 0, (samplesRequested - samplesWritten) * sizeof(int16_t));
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
