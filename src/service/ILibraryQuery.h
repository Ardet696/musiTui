#ifndef MP3PLAYER_ILIBRARYQUERY_H
#define MP3PLAYER_ILIBRARYQUERY_H
#include <cstdint>
#include <string>
#include <vector>

class ILibraryQuery
{
    public:
        virtual ~ILibraryQuery() = default;
        virtual std::vector<std::string> getAlbumNames() const = 0;
        virtual std::vector<bool> getAlbumLoadedStates() const = 0;
        virtual std::uint32_t getDataVersion() const = 0;
        virtual std::vector<std::string> getSongNames(const std::string& album) const = 0;
        virtual std::vector<std::vector<std::string>> getAllSongNames() const = 0;
        virtual bool setRootPath(const std::string& path, std::string& outError) = 0;
        virtual std::string getCurrentAlbum() const = 0;
        virtual int getCurrentTrackIndex() const = 0;
        virtual std::string getCurrentSongName() const = 0;
        virtual std::string getNextSongName() const = 0;
};
#endif // MP3PLAYER_ILIBRARYQUERY_H
