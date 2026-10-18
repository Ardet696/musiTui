#ifndef MP3PLAYER_PLAYERCORE_H
#define MP3PLAYER_PLAYERCORE_H

#include "commands/CommandQueue.h"
#include "events/NotificationBus.h"
#include "library/MusicLibrary.h"
#include "player/PlaybackController.h"
#include "service/AsyncLibraryService.h"
#include "service/ConfigService.h"
#include "service/LibraryService.h"

/**
 * PlayerCore - everything the player needs regardless of front end.
 * Owned by the TUI App and by the headless Daemon.
 */
class PlayerCore {
public:
    PlayerCore();

    PlayerCore(const PlayerCore&) = delete;
    PlayerCore& operator=(const PlayerCore&) = delete;
    PlayerCore(PlayerCore&&) = delete;
    PlayerCore& operator=(PlayerCore&&) = delete;

    // Mutations are queued, so it is safe to call from any thread.
    ILibraryService& service() { return asyncService_; }
    // Synchronous service, for callers already off the UI thread.
    ILibraryService& directService() { return service_; }
    IConfigService& config() { return config_; }
    NotificationBus& bus() { return bus_; }

private:
    NotificationBus bus_;
    ConfigService config_;
    MusicLibrary library_;
    CommandQueue cmdQueue_;
    PlaybackController controller_;
    LibraryService service_;
    AsyncLibraryService asyncService_;
};

#endif
