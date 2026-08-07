#include "LibraryService.h"
#include "../events/NotificationBus.h"
#include <algorithm>
#include <shared_mutex>

LibraryService::LibraryService(MusicLibrary& library, PlaybackController& controller, NotificationBus& bus)
    : library_(library), controller_(controller), bus_(bus) {}

LibraryService::~LibraryService() {
    std::lock_guard control(loadControlMutex_);
    stopBackgroundLoad();
}

NotificationBus& LibraryService::getNotificationBus() { return bus_; }

std::vector<std::string> LibraryService::getAlbumNames() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    for (const auto& album : library_.getAlbums()) {
        names.push_back(album.getTitle());
    }
    return names;
}

std::vector<bool> LibraryService::getAlbumLoadedStates() const {
    std::shared_lock lock(mutex_);
    std::vector<bool> states;
    for (const auto& album : library_.getAlbums()) {
        states.push_back(album.isLoaded());
    }
    return states;
}

std::uint32_t LibraryService::getDataVersion() const {
    return dataVersion_.load();
}

std::vector<std::string> LibraryService::getSongNames(const std::string& albumName) const {
    std::shared_lock lock(mutex_);
    const auto* album = library_.findAlbumByTitle(albumName);
    if (!album) return {};

    std::vector<std::string> names;
    for (const auto& song : album->getSongs()) {
        names.push_back(song.getTitle());
    }
    return names;
}

std::vector<std::vector<std::string>> LibraryService::getAllSongNames() const {
    std::shared_lock lock(mutex_);
    std::vector<std::vector<std::string>> all;
    for (const auto& album : library_.getAlbums()) {
        std::vector<std::string> names;
        for (const auto& song : album.getSongs()) {
            names.push_back(song.getTitle());
        }
        all.push_back(std::move(names));
    }
    return all;
}

bool LibraryService::setRootPath(const std::string& path, std::string& outError) {
    MusicLibrary newLibrary = scanner_.scanFromUserInput(path, &outError);
    if (!outError.empty()) {
        return false;
    }

    std::lock_guard control(loadControlMutex_);
    stopBackgroundLoad();

    {
        std::lock_guard lock(mutex_);
        library_ = std::move(newLibrary);
    }
    dataVersion_.fetch_add(1);

    startBackgroundLoad();
    return true;
}

void LibraryService::startBackgroundLoad() {
    std::vector<std::filesystem::path> paths;
    {
        std::shared_lock lock(mutex_);
        paths = library_.getAlbumPaths();
    }
    if (paths.empty()) {
        return;
    }

    bus_.push("Tip: keep albums small for a faster launch");

    cancelLoad_.store(false);
    nextAlbum_.store(0);

    const unsigned hw = std::thread::hardware_concurrency();
    const unsigned workerCount = std::min<unsigned>(
        paths.size(), std::clamp<unsigned>(hw, 2u, 4u));

    auto sharedPaths = std::make_shared<std::vector<std::filesystem::path>>(std::move(paths));
    for (unsigned w = 0; w < workerCount; ++w) {
        loaders_.emplace_back([this, sharedPaths] {
            const int total = static_cast<int>(sharedPaths->size());
            for (int i = nextAlbum_.fetch_add(1); i < total; i = nextAlbum_.fetch_add(1)) {
                if (cancelLoad_.load()) return;
                Album full((*sharedPaths)[i]);  // heavy decode, off-lock
                if (cancelLoad_.load()) return;
                {
                    std::lock_guard lock(mutex_);
                    library_.replaceAlbum(i, std::move(full));
                }
                dataVersion_.fetch_add(1);
            }
        });
    }
}

void LibraryService::stopBackgroundLoad() {
    cancelLoad_.store(true);
    for (auto& t : loaders_) {
        if (t.joinable()) t.join();
    }
    loaders_.clear();
}

void LibraryService::playSong(const std::string& albumName, int songIndex) {
    std::vector<std::filesystem::path> songPaths;
    {
        std::shared_lock lock(mutex_);
        const auto* album = library_.findAlbumByTitle(albumName);
        if (!album) return;

        if (!album->isLoaded()) {
            bus_.push("Album still loading…");
            return;
        }

        const auto& songs = album->getSongs();
        if (songIndex < 0 || songIndex >= static_cast<int>(songs.size())) return;

        songPaths.reserve(songs.size() - songIndex);
        for (int i = songIndex; i < static_cast<int>(songs.size()); ++i) {
            songPaths.push_back(songs[i].getFilePath());
        }
    }

    controller_.setCurrentAlbum(albumName);
    controller_.loadAlbum(songPaths);
    controller_.playSongAtIndex(0);
}

void LibraryService::prevSong() {
    controller_.previous();
}

void LibraryService::nextSong() {
    controller_.next();
}

void LibraryService::resumePlayback() {
    controller_.play();
}

void LibraryService::stopPlayback() {
    controller_.pause();
}

std::vector<float> LibraryService::getSpectrumBars() const {
    return controller_.getSpectrumBars();
}
std::vector<float> LibraryService::getSpectrumMagnitudes() const
{   return controller_.getSpectrumMagnitudes();
}

float LibraryService::getPlaybackProgress() const {
    return controller_.getPlaybackProgress();
}

float LibraryService::getBpmLoadFactor() const {
    return controller_.getBpmLoadFactor();
}

std::string LibraryService::getCurrentAlbum() const {
    return controller_.getCurrentAlbum();
}

int LibraryService::getCurrentTrackIndex() const {
    return controller_.getCurrentTrackNumber() - 1;
}

std::string LibraryService::getCurrentSongName() const {
    auto path = controller_.getCurrentSong();
    if (path.empty()) return "";
    return path.stem().string();
}

std::string LibraryService::getNextSongName() const {
    auto path = controller_.getNextSongPath();
    if (path.empty()) return "";
    return path.stem().string();
}

void LibraryService::setVolume(int percent) {
    controller_.setVolume(percent);
}

int LibraryService::getVolume() const {
    return controller_.getVolume();
}

std::vector<std::string> LibraryService::listOutputDevices() const {
    return PlaybackController::listOutputDevices();
}

void LibraryService::setOutputDevice(int deviceIndex) {
    auto devices = PlaybackController::listOutputDevices();
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size())) {
        bus_.push("Output device no longer available", NotifyLevel::Error);
        return;
    }
    controller_.setOutputDevice(devices[deviceIndex]);
}
