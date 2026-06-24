#include "Album.h"
#include <algorithm>

Album::Album(const std::filesystem::path& dirPath)
    : Album(dirPath, SkeletonTag{})
{
    load();
}

Album::Album(const std::filesystem::path& dirPath, SkeletonTag)
    : dirPath_(dirPath)
    , title_(dirPath.filename().string())  // Directory name as album title
    , artist_("Unknown")
    , type_(AlbumType::Single)
{
}

void Album::load() {
    loadSongsFromDirectory();
    type_ = classifyByNumSongs(static_cast<int>(songs_.size()));
    loaded_ = true;
}

void Album::loadSongsFromDirectory() {
    if (!std::filesystem::exists(dirPath_) || !std::filesystem::is_directory(dirPath_)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath_)) {
        if (entry.is_directory()) {
            continue;
        }

        if (entry.is_regular_file() && entry.path().extension() == ".mp3") {
            try {
                songs_.emplace_back(entry.path(), title_);
            } catch (const std::exception&) {
            }
        }
    }

    std::sort(songs_.begin(), songs_.end(), [](const Song& a, const Song& b) {
        return a.getFilePath().filename() < b.getFilePath().filename();
    });
}

Album::AlbumType Album::classifyByNumSongs(int numSongs) {
    if (numSongs <= 3) {
        return AlbumType::Single;
    } else if (numSongs <= 7) {
        return AlbumType::EP;
    } else {
        return AlbumType::StudioAlbum;
    }
}

std::string Album::getTypeAsString() const {
    switch (type_) {
        case AlbumType::Single: return "Single";
        case AlbumType::EP: return "EP";
        case AlbumType::StudioAlbum: return "Studio Album";
        default: return "Unknown";
    }
}

int Album::getTotalDurationSeconds() const {
    int total = 0;
    for (const auto& song : songs_) {
        total += song.getDurationSeconds();
    }
    return total;
}

std::string Album::getFormattedTotalDuration() const {
    const int totalSeconds = getTotalDurationSeconds();
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    } else {
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
}

const Song* Album::getSongByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(songs_.size())) {
        return nullptr;
    }
    return &songs_[index];
}
