#include "AlbumInfo.h"
#include "../decode/AudioDecoderFactory.h"
#include <algorithm>

AlbumInfo::AlbumInfo(const std::filesystem::path& dirPath)
    : dirPath_(dirPath)
    , title_(dirPath.filename().string())
    , songsLoaded_(false)
{
}

int AlbumInfo::loadSongNames() {
    if (songsLoaded_) {
        return static_cast<int>(songFilenames_.size());
    }

    songFilenames_.clear();

    if (!std::filesystem::exists(dirPath_) || !std::filesystem::is_directory(dirPath_)) {
        return 0;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath_)) {
        if (entry.is_regular_file() && AudioDecoderFactory::isSupported(entry.path())) {
            songFilenames_.push_back(entry.path().filename().string());
        }
    }

    std::sort(songFilenames_.begin(), songFilenames_.end());

    songsLoaded_ = true;
    return static_cast<int>(songFilenames_.size());
}

std::filesystem::path AlbumInfo::getSongPath(int index) const {
    if (index < 0 || index >= static_cast<int>(songFilenames_.size())) {
        return {};
    }
    return dirPath_ / songFilenames_[index];
}
