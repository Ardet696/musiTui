#ifndef MP3PLAYER_SONGQUEUE_H
#define MP3PLAYER_SONGQUEUE_H

#include <filesystem>
#include <vector>
#include <deque>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include "../decode/AudioDecoderFactory.h"
#include "../config/Config.h"

class NotificationBus;

/**
 * SongQueue - Prewarms upcoming songs for instant transitions.
 *
 * Strategy: Keep 2-3 songs "pre decoded" and validated, when current song ends, load a pre decoded song.
 *   - Handles skipping by discarding irrelevant pre decoded songs
 *
 * Memory Usage:
 *   - Only stores decoders (file handles), it saves memory by not storing the heavy PCM's.
 *   - 3 decoders at 1KB each ~= 3KB compared to hundred's of MB of PCM.
 */
class SongQueue {
public:
    explicit SongQueue(NotificationBus* bus = nullptr);
    ~SongQueue();

    SongQueue(const SongQueue&) = delete;
    SongQueue& operator=(const SongQueue&) = delete;
    SongQueue(SongQueue&&) = delete;
    SongQueue& operator=(SongQueue&&) = delete;

    /**
     * Load playlist of songs.
     * @param songPaths - Full paths to MP3 files in play order
     */
    void loadPlaylist(const std::vector<std::filesystem::path>& songPaths);

    std::filesystem::path getCurrentSongPath() const;

    /**
     * Advance to next song.
     * @return Path to next song, or empty if at end of playlist
     */
    std::filesystem::path next();

    /**
     * Go to previous song.
     * @return Path to previous song, or empty if at beginning
     */
    std::filesystem::path previous();

    /**
     * Skip to specific index.
     * @param index - Song index in playlist
     * @return Path to song, or empty if invalid index
     */
    std::filesystem::path skipTo(int index);

    /**
     * Check if decoder for next song is pre decoded and ready.
     */
    bool isNextSongReady() const;

    /**
     * Get current playlist position.
     */
    int getCurrentIndex() const { return currentIndex_.load(std::memory_order_relaxed); }
    int getPlaylistSize() const { return static_cast<int>(playlist_.size()); }
    std::filesystem::path getNextSongPath() const;

    /**
     * Clear playlist and stop pre decoding
     */
    void clear();

private:
    /**
     * Background thread that pre decodes upcoming songs.
     */
    void preWarmLoop(std::stop_token stopToken);

    /**
     * Pre decode a specific song decoding the first few frames.
     */
    bool preWarmSong(const std::filesystem::path& songPath);

    std::vector<std::filesystem::path> playlist_;
    std::atomic<int> currentIndex_;

    std::atomic<int> preWarmedUpTo_;  // Index of last pre decoded song
    std::jthread preWarmThread_;
    mutable std::mutex mutex_;
    NotificationBus* bus_;
};

#endif // MP3PLAYER_SONGQUEUE_H
