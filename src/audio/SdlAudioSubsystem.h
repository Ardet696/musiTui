#ifndef MP3PLAYER_SDLAUDIOSUBSYSTEM_H
#define MP3PLAYER_SDLAUDIOSUBSYSTEM_H

// Handle to the process-wide SDL audio subsystem.
//
// The subsystem is initialized once on first use and stays up for the rest of
// the process. It is deliberately NOT torn down when the last handle dies:
// every track change destroys the sink and builds a new one, and bouncing
// SDL_QuitSubSystem/SDL_InitSubSystem in that window drops the PipeWire/Pulse
// client connection, so the following SDL_OpenAudioDevice races the server
// releasing it and intermittently fails.
class SdlAudioSubsystem {
public:
    SdlAudioSubsystem();
    ~SdlAudioSubsystem() = default;

    bool ok() const { return ok_; }

    SdlAudioSubsystem(const SdlAudioSubsystem&) = delete;
    SdlAudioSubsystem& operator=(const SdlAudioSubsystem&) = delete;
    SdlAudioSubsystem(SdlAudioSubsystem&&) = delete;
    SdlAudioSubsystem& operator=(SdlAudioSubsystem&&) = delete;

private:
    bool ok_ = false;
};

#endif
