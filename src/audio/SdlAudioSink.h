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

struct _SDL_AudioStream;

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
    bool setSourceFormat(const AudioFormat& fmt) override;
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

    // Callers must hold the device lock (or not have started the device yet).
    bool rebuildConverter();

    SdlAudioSubsystem audio_;
    std::uintptr_t device_ = 0; // SDL_AudioDeviceID stored portably
    AudioFormat srcFmt_{};      // what the decoder produces
    AudioFormat deviceFmt_{};   // what the device actually runs at
    FrameProvider provider_{};
    bool open_ = false;
    std::atomic<int> volume_{100}; // 0-100

    // Null when source and device formats match, in which case the callback
    // fills the device buffer straight from the provider.
    _SDL_AudioStream* converter_ = nullptr;
    std::vector<std::int16_t> srcBuffer_; // preallocated so fill() never allocates
    std::size_t srcFramesPerPull_ = 0;
    int deviceBufferBytes_ = 0;

    std::vector<std::uint8_t> mixBuffer_;
    NotificationBus* bus_;
};

#endif
