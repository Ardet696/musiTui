#include "MusicLibrary.h"

void MusicLibrary::addAlbum(Album album) {
    albums_.push_back(std::move(album));
}

void MusicLibrary::addPlaylist(Playlist playlist) {
    playlists_.push_back(std::move(playlist));
}

int MusicLibrary::getTotalSongs() const {
    int total = 0;
    for (const auto& album : albums_) {
        total += album.getNumSongs();
    }
    for (const auto& playlist : playlists_) {
        total += playlist.getNumSongs();
    }
    return total;
}

void MusicLibrary::replaceAlbum(int index, Album album) {
    if (index < 0 || index >= static_cast<int>(albums_.size())) {
        return;
    }
    albums_[index] = std::move(album);
}

std::vector<std::filesystem::path> MusicLibrary::getAlbumPaths() const {
    std::vector<std::filesystem::path> paths;
    paths.reserve(albums_.size());
    for (const auto& album : albums_) {
        paths.push_back(album.getPath());
    }
    return paths;
}

const Album* MusicLibrary::getAlbumByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(albums_.size())) {
        return nullptr;
    }
    return &albums_[index];
}

const Playlist* MusicLibrary::getPlaylistByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(playlists_.size())) {
        return nullptr;
    }
    return &playlists_[index];
}

const Album* MusicLibrary::findAlbumByTitle(const std::string& title) const {
    for (const auto& album : albums_) {
        if (album.getTitle() == title) {
            return &album;
        }
    }
    return nullptr;
}

const Playlist* MusicLibrary::findPlaylistByTitle(const std::string& title) const {
    for (const auto& playlist : playlists_) {
        if (playlist.getTitle() == title) {
            return &playlist;
        }
    }
    return nullptr;
}

void MusicLibrary::clear() {
    albums_.clear();
    playlists_.clear();
}
