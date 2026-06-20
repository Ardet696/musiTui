#ifndef MP3PLAYER_SDLAUDIOSINK_H
#define MP3PLAYER_SDLAUDIOSINK_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../util/AudioFormat.h"
#include "IAudioSink.h"
#include "SdlAudioSubsystem.h"

class NotificationBus;

class SdlAudioSink : public IAudioSink {
public:
    using FrameProvider = IAudioSink::FrameProvider;

    explicit SdlAudioSink(NotificationBus* bus = nullptr);
    ~SdlAudioSink() override;

    SdlAudioSink(const SdlAudioSink&) = delete;
    SdlAudioSink& operator=(const SdlAudioSink&) = delete;
    SdlAudioSink(SdlAudioSink&&) = delete;
    SdlAudioSink& operator=(SdlAudioSink&&) = delete;

    bool open(const AudioFormat& fmt, FrameProvider provider, const std::string& deviceName = "", int desiredBufferFrames = 2048) override;
    void start() override;
    void stop() override;
    void close() override;
    bool isOpen() const override;

    void setVolume(int percent) override;   // 0-100
    int  getVolume() const override;

    /// List available audio output device names via SDL.
    static std::vector<std::string> listOutputDevices();

private:
    static void sdlCallback(void* userdata, std::uint8_t* stream, int len);
    void fill(std::uint8_t* stream, int len);

    SdlAudioSubsystem audio_;
    std::uintptr_t device_ = 0; // SDL_AudioDeviceID stored portably
    AudioFormat fmt_{};
    FrameProvider provider_{};
    bool open_ = false;
    std::atomic<int> volume_{100}; // 0-100
    std::vector<std::uint8_t> mixBuffer_;
    NotificationBus* bus_;
};

#endif
