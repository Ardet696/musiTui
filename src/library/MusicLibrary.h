#ifndef MP3PLAYER_MUSICLIBRARY_H
#define MP3PLAYER_MUSICLIBRARY_H

#include <vector>
#include <string>
#include <filesystem>
#include "Album.h"
#include "Playlist.h"

class MusicLibrary {
public:
    MusicLibrary() = default;

    void addAlbum(Album album);
    void addPlaylist(Playlist playlist);

    const std::vector<Album>& getAlbums() const { return albums_; }
    const std::vector<Playlist>& getPlaylists() const { return playlists_; }

    int getNumAlbums() const { return static_cast<int>(albums_.size()); }
    int getNumPlaylists() const { return static_cast<int>(playlists_.size()); }
    int getTotalSongs() const;

    void replaceAlbum(int index, Album album);
    std::vector<std::filesystem::path> getAlbumPaths() const;

    const Album* getAlbumByIndex(int index) const;
    const Playlist* getPlaylistByIndex(int index) const;
    const Album* findAlbumByTitle(const std::string& title) const;
    const Playlist* findPlaylistByTitle(const std::string& title) const;

    void clear();

private:
    std::vector<Album> albums_;
    std::vector<Playlist> playlists_;
};

#endif // MP3PLAYER_MUSICLIBRARY_H
