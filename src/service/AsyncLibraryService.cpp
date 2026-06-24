#include "AsyncLibraryService.h"

AsyncLibraryService::AsyncLibraryService(ILibraryService& real, CommandQueue& queue)
    : real_(real), queue_(queue)
{}

void AsyncLibraryService::playSong(const std::string& album, int songIndex) {
    queue_.enqueue([this, album, songIndex] { real_.playSong(album, songIndex); });
}

void AsyncLibraryService::prevSong() {
    queue_.enqueue([this] { real_.prevSong(); });
}

void AsyncLibraryService::nextSong() {
    queue_.enqueue([this] { real_.nextSong(); });
}

void AsyncLibraryService::resumePlayback() {
    queue_.enqueue([this] { real_.resumePlayback(); });
}

void AsyncLibraryService::stopPlayback() {
    queue_.enqueue([this] { real_.stopPlayback(); });
}

void AsyncLibraryService::setVolume(int percent) {
    queue_.enqueue([this, percent] { real_.setVolume(percent); });
}

void AsyncLibraryService::setOutputDevice(int deviceIndex) {
    queue_.enqueue([this, deviceIndex] { real_.setOutputDevice(deviceIndex); });
}

bool AsyncLibraryService::setRootPath(const std::string& path, std::string& outError) {
    return real_.setRootPath(path, outError);
}

std::vector<std::string> AsyncLibraryService::getAlbumNames() const {
    return real_.getAlbumNames();
}

std::vector<bool> AsyncLibraryService::getAlbumLoadedStates() const {
    return real_.getAlbumLoadedStates();
}

std::uint32_t AsyncLibraryService::getDataVersion() const {
    return real_.getDataVersion();
}

std::vector<std::string> AsyncLibraryService::getSongNames(const std::string& album) const {
    return real_.getSongNames(album);
}

std::vector<std::vector<std::string>> AsyncLibraryService::getAllSongNames() const {
    return real_.getAllSongNames();
}

std::vector<float> AsyncLibraryService::getSpectrumBars() const {
    return real_.getSpectrumBars();
}

std::vector<float> AsyncLibraryService::getSpectrumMagnitudes() const {
    return real_.getSpectrumMagnitudes();
}

float AsyncLibraryService::getPlaybackProgress() const {
    return real_.getPlaybackProgress();
}

float AsyncLibraryService::getBpmLoadFactor() const {
    return real_.getBpmLoadFactor();
}

std::string AsyncLibraryService::getCurrentAlbum() const {
    return real_.getCurrentAlbum();
}

int AsyncLibraryService::getCurrentTrackIndex() const {
    return real_.getCurrentTrackIndex();
}

std::string AsyncLibraryService::getCurrentSongName() const {
    return real_.getCurrentSongName();
}

std::string AsyncLibraryService::getNextSongName() const {
    return real_.getNextSongName();
}

int AsyncLibraryService::getVolume() const {
    return real_.getVolume();
}

std::vector<std::string> AsyncLibraryService::listOutputDevices() const {
    return real_.listOutputDevices();
}

NotificationBus& AsyncLibraryService::getNotificationBus() {
    return real_.getNotificationBus();
}
