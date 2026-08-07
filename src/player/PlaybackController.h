#ifndef MP3PLAYER_PLAYBACKCONTROLLER_H
#define MP3PLAYER_PLAYBACKCONTROLLER_H

#include <filesystem>
#include <vector>
#include "PlaybackEngine.h"
#include "SongQueue.h"
#include "AutoAdvanceManager.h"
#include "../events/PlaybackEventPublisher.h"
#include "../commands/CommandQueue.h"

class NotificationBus;

/**
 * PlaybackController - High-level playback orchestration.
 *
 * Coordinates PlaybackEngine, SongQueue, and AutoAdvanceManager.
 * Publishes events for UI/logging via PlaybackEventPublisher.
*/
class PlaybackController {
public:
    explicit PlaybackController(NotificationBus& bus, CommandQueue& cmdQueue);
    ~PlaybackController();

    // Non-copyable
    PlaybackController(const PlaybackController&) = delete;
    PlaybackController& operator=(const PlaybackController&) = delete;

    void loadAlbum(const std::vector<std::filesystem::path>& songPaths);
    void play();
    void pause();
    void stop();
    void next();
    void previous();

    /**
     * Play a specific song from the current album.
     * Loads album if not already loaded, then skips to index and plays.
     * After this song finishes, playback continues with next songs.
     */
    void playSongAtIndex(int index);


    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    std::filesystem::path getCurrentSong() const;
    std::filesystem::path getNextSongPath() const;
    int getCurrentTrackNumber() const;
    int getTotalTracks() const;

    std::vector<float> getSpectrumBars() const;
    std::vector<float> getSpectrumMagnitudes() const;
    float getPlaybackProgress() const;
    float getBpmLoadFactor() const;

    std::string getCurrentAlbum() const;
    void setCurrentAlbum(const std::string& albumName);

    void setVolume(int percent);  // 0-100
    int  getVolume() const;

    /// Set the preferred audio output device. Reloads the current song through the new device.
    bool setOutputDevice(const std::string& deviceName);
    std::string getOutputDevice() const;
    static std::vector<std::string> listOutputDevices();

    /**
     * Event subscription.
     */
    PlaybackEventPublisher& getEventPublisher();

private:
    bool loadAndPlaySong(const std::filesystem::path& songPath);
    void handleAutoAdvance();
    bool shouldAutoAdvance() const;

    PlaybackEngine engine_;
    SongQueue queue_;
    AutoAdvanceManager autoAdvance_;
    PlaybackEventPublisher eventPublisher_;
    std::vector<std::filesystem::path> currentAlbum_;
    std::string currentAlbumName_;

    NotificationBus& bus_;
    CommandQueue& cmdQueue_;
};

#endif