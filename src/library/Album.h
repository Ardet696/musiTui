#ifndef MP3PLAYER_ALBUM_H
#define MP3PLAYER_ALBUM_H

#include <string>
#include <vector>
#include <filesystem>
#include "Song.h"


class Album {
public:
    enum class AlbumType {
        Single,
        EP,
        StudioAlbum
    };

    struct SkeletonTag {};

    explicit Album(const std::filesystem::path& dirPath);
    Album(const std::filesystem::path& dirPath, SkeletonTag);

    void load();
    bool isLoaded() const { return loaded_; }

    std::string getTitle() const { return title_; }
    std::string getArtist() const { return artist_; }
    AlbumType getType() const { return type_; }
    int getNumSongs() const { return static_cast<int>(songs_.size()); }
    const std::vector<Song>& getSongs() const { return songs_; }
    std::filesystem::path getPath() const { return dirPath_; }

    void setTitle(const std::string& title) { title_ = title; }
    void setArtist(const std::string& artist) { artist_ = artist; }

    std::string getTypeAsString() const;
    int getTotalDurationSeconds() const;
    std::string getFormattedTotalDuration() const;

    const Song* getSongByIndex(int index) const;

private:

    static AlbumType classifyByNumSongs(int numSongs);
    void loadSongsFromDirectory();

    std::filesystem::path dirPath_;
    std::string title_;
    std::string artist_;
    AlbumType type_;
    std::vector<Song> songs_;
    bool loaded_ = false;
};

#endif
