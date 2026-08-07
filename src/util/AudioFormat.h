#ifndef MP3PLAYER_AUDIOFORMAT_H
#define MP3PLAYER_AUDIOFORMAT_H

#include <cstdint>

struct AudioFormat {
    int sampleRate = 0;
    int channels   = 0;

    friend bool operator==(const AudioFormat&, const AudioFormat&) = default;
};

#endif // MP3PLAYER_AUDIOFORMAT_H
