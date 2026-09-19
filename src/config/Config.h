#ifndef MP3PLAYER_CONFIG_H
#define MP3PLAYER_CONFIG_H

#include <cstddef>
#include <chrono>

/**
 * Config - Centralized configuration for the MP3 Player
 *
 * All tunable parameters in one place for easy adjustment and future configuration file support
 */
class Config {
public:
    static constexpr int END_OF_STREAM_THRESHOLD = 5;
    static constexpr std::size_t RING_BUFFER_SIZE_SECONDS = 2;
    // Worst case decoder output, used to size the ring buffer once up front.
    static constexpr std::size_t MAX_SAMPLE_RATE = 192000;
    static constexpr std::size_t MAX_CHANNELS = 2;
    static constexpr std::size_t DECODE_CHUNK_FRAMES = 1152;
    static constexpr std::chrono::milliseconds DECODE_THREAD_SLEEP_MS{5};
    static constexpr int PRE_WARM_AHEAD_COUNT = 3;
    static constexpr int PRE_WARM_VALIDATION_FRAMES = 10;
    static constexpr std::chrono::milliseconds PRE_WARM_POLL_INTERVAL_MS{100};
    static constexpr std::chrono::milliseconds AUTO_ADVANCE_POLL_INTERVAL_MS{100};
    static constexpr int ESTIMATED_MP3_BITRATE = 192000;
    static constexpr float RING_BUFFER_PREFILL_PERCENT = 0.25f;

};

#endif
