#ifndef MP3PLAYER_IPLAYBACKCONTROL_H
#define MP3PLAYER_IPLAYBACKCONTROL_H
#include <string>
#include <vector>

class IPlaybackControl
{
    public:
        virtual ~IPlaybackControl() = default;
        virtual void playSong(const std::string& album, int songIndex) = 0;
        virtual void prevSong() = 0;
        virtual void nextSong() = 0;
        virtual void resumePlayback() = 0;
        virtual void stopPlayback() = 0;
        virtual bool isPlaying() const = 0;
        virtual void setVolume(int percent) = 0;
        virtual int  getVolume() const = 0;
        virtual std::vector<std::string> listOutputDevices() const = 0;
        virtual void setOutputDevice(int deviceIndex) = 0;
};
#endif // MP3PLAYER_IPLAYBACKCONTROL_H
