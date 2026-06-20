#ifndef MP3PLAYER_IAUDIOSINK_H
#define MP3PLAYER_IAUDIOSINK_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "../util/AudioFormat.h"

class IAudioSink {
public:
    using FrameProvider = std::function<std::size_t(int16_t* dstInterleaved, std::size_t framesRequested)>;

    virtual ~IAudioSink() = default;

    virtual bool open(const AudioFormat& fmt, FrameProvider provider, const std::string& deviceName = "", int desiredBufferFrames = 2048) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual void setVolume(int percent) = 0;
    virtual int  getVolume() const = 0;
};

#endif // MP3PLAYER_IAUDIOSINK_H
