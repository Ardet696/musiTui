#include "PlaybackController.h"
#include "../events/NotificationBus.h"

PlaybackController::PlaybackController(NotificationBus& bus, CommandQueue& cmdQueue)
    : engine_(&bus)
    , queue_(&bus)
    , bus_(bus)
    , cmdQueue_(cmdQueue)
{
}

PlaybackController::~PlaybackController() {
    stop();
}

void PlaybackController::loadAlbum(const std::vector<std::filesystem::path>& songPaths) {
    stop();

    currentAlbum_ = songPaths;
    queue_.loadPlaylist(songPaths);

    if (!songPaths.empty()) {
        bus_.push("Album loaded: " + std::to_string(songPaths.size()) + " tracks");

        autoAdvance_.start(
            [this]() { return shouldAutoAdvance(); },
            [this]() { cmdQueue_.enqueue([this]() { handleAutoAdvance(); }); }
        );

        eventPublisher_.publish(PlaybackEvent(PlaybackEventType::TrackChanged));
    }
}

void PlaybackController::play() {
    if (queue_.getPlaylistSize() == 0) {
        bus_.push("No album loaded", NotifyLevel::Error);
        return;
    }

    if (engine_.isPaused()) {
        engine_.play();
        eventPublisher_.publish(PlaybackEvent(PlaybackEventType::SongResumed));
        return;
    }

    const std::filesystem::path currentSong = queue_.getCurrentSongPath();
    if (currentSong.empty()) {
        bus_.push("No current song", NotifyLevel::Error);
        return;
    }

    loadAndPlaySong(currentSong);
}

void PlaybackController::pause() {
    engine_.pause();
    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::SongPaused));
}

void PlaybackController::stop() {
    engine_.stop();
    queue_.clear();
    autoAdvance_.stop();
    currentAlbum_.clear();
    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::SongStopped));
}

void PlaybackController::next() {
    const std::filesystem::path nextSong = queue_.next();
    if (nextSong.empty()) {
        bus_.push("End of album reached");
        engine_.stop();
        queue_.clear();
        eventPublisher_.publish(PlaybackEvent(PlaybackEventType::AlbumFinished));
        return;
    }

    loadAndPlaySong(nextSong);
    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::TrackChanged,nextSong,getCurrentTrackNumber(),getTotalTracks()));
}

void PlaybackController::previous() {
    const std::filesystem::path prevSong = queue_.previous();
    if (prevSong.empty()) {
        bus_.push("Already at first song");
        return;
    }

    loadAndPlaySong(prevSong);
    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::TrackChanged,prevSong,getCurrentTrackNumber(),  getTotalTracks()));
}

bool PlaybackController::isPlaying() const {
    return engine_.isPlaying();
}

bool PlaybackController::isPaused() const {
    return engine_.isPaused();
}

bool PlaybackController::isStopped() const {
    return engine_.isStopped();
}

std::filesystem::path PlaybackController::getCurrentSong() const {
    return queue_.getCurrentSongPath();
}

std::filesystem::path PlaybackController::getNextSongPath() const {
    return queue_.getNextSongPath();
}

int PlaybackController::getCurrentTrackNumber() const {
    return queue_.getCurrentIndex() + 1;  // 1-indexed for user display
}

int PlaybackController::getTotalTracks() const {
    return queue_.getPlaylistSize();
}

std::vector<float> PlaybackController::getSpectrumBars() const
{
    return engine_.getSpectrumAnalyzer().getBars();
}

std::vector<float> PlaybackController::getSpectrumMagnitudes() const
{
    return engine_.getSpectrumAnalyzer().getSpectrumMagnitudes();
}

float PlaybackController::getPlaybackProgress() const {
    return engine_.getPlaybackProgress();
}

float PlaybackController::getBpmLoadFactor() const {
    return engine_.getBpmLoadFactor();
}

std::string PlaybackController::getCurrentAlbum() const {
    return currentAlbumName_;
}

void PlaybackController::setCurrentAlbum(const std::string& albumName) {
    currentAlbumName_ = albumName;
}

PlaybackEventPublisher& PlaybackController::getEventPublisher() {
    return eventPublisher_;
}

bool PlaybackController::loadAndPlaySong(const std::filesystem::path& songPath) {
    // This function has to  be called with mutex_ held!

    engine_.stop();

    if (!engine_.load(songPath)) {
        bus_.push("Failed to load: " + songPath.filename().string(), NotifyLevel::Error);
        eventPublisher_.publish(PlaybackEvent(PlaybackEventType::LoadFailed, songPath, 0, 0, "Failed to load song"));
        return false;
    }

    bus_.push("Now playing: " + songPath.filename().string());

    engine_.play();

    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::SongStarted,songPath,getCurrentTrackNumber(), getTotalTracks()));
    return true;
}

void PlaybackController::playSongAtIndex(int index) {
    if (queue_.getPlaylistSize() == 0) {
        bus_.push("No album loaded", NotifyLevel::Error);
        return;
    }

    auto songPath = queue_.skipTo(index);
    if (songPath.empty()) {
        bus_.push("Invalid index: " + std::to_string(index), NotifyLevel::Error);
        return;
    }

    loadAndPlaySong(songPath);
}

void PlaybackController::handleAutoAdvance() {
    bus_.push("Auto-advancing...");

    const std::filesystem::path nextSong = queue_.next();
    if (nextSong.empty()) {
        bus_.push("End of album reached");
        engine_.stop();
        queue_.clear();
        eventPublisher_.publish(PlaybackEvent(PlaybackEventType::AlbumFinished));
        return;
    }

    loadAndPlaySong(nextSong);
    eventPublisher_.publish(PlaybackEvent(PlaybackEventType::SongFinished));
}

void PlaybackController::setVolume(int percent) {
    engine_.setVolume(percent);
}

int PlaybackController::getVolume() const {
    return engine_.getVolume();
}

bool PlaybackController::setOutputDevice(const std::string& deviceName) {
    // The engine hot-swaps the sink in place, so playback keeps its position
    // and a device that cannot be opened leaves the current one running.
    return engine_.setOutputDevice(deviceName);
}

std::string PlaybackController::getOutputDevice() const {
    return engine_.getOutputDevice();
}

std::vector<std::string> PlaybackController::listOutputDevices() {
    return PlaybackEngine::listOutputDevices();
}

bool PlaybackController::shouldAutoAdvance() const {
    return engine_.isPlaying() && engine_.hasReachedEndOfStream();
}