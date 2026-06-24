#pragma once

#include "ILibraryService.h"
#include "../commands/CommandQueue.h"

class AsyncLibraryService : public ILibraryService {
public:
    AsyncLibraryService(ILibraryService& real, CommandQueue& queue);

    // Mutating  enqueued (TUI thread returns immediately)
    void playSong(const std::string& album, int songIndex) override;
    void prevSong() override;
    void nextSong() override;
    void resumePlayback() override;
    void stopPlayback() override;
    void setVolume(int percent) override;
    void setOutputDevice(int deviceIndex) override;

    // Synchronous returns a result the caller uses immediately
    bool setRootPath(const std::string& path, std::string& outError) override;

    std::vector<std::string> getAlbumNames() const override;
    std::vector<bool> getAlbumLoadedStates() const override;
    std::uint32_t getDataVersion() const override;
    std::vector<std::string> getSongNames(const std::string& album) const override;
    std::vector<std::vector<std::string>> getAllSongNames() const override;
    std::vector<float> getSpectrumBars() const override;
    std::vector<float> getSpectrumMagnitudes() const override;
    float getPlaybackProgress() const override;
    float getBpmLoadFactor() const override;
    std::string getCurrentAlbum() const override;
    int getCurrentTrackIndex() const override;
    std::string getCurrentSongName() const override;
    std::string getNextSongName() const override;
    int getVolume() const override;
    std::vector<std::string> listOutputDevices() const override;
    NotificationBus& getNotificationBus() override;

private:
    ILibraryService& real_;
    CommandQueue& queue_;
};
