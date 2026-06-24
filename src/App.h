#ifndef MP3PLAYER_APP_H
#define MP3PLAYER_APP_H

#include <atomic>
#include <memory>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>

#include "commands/CommandQueue.h"
#include "events/NotificationBus.h"
#include "library/MusicLibrary.h"
#include "player/PlaybackController.h"
#include "service/AsyncLibraryService.h"
#include "service/ConfigService.h"
#include "service/LibraryService.h"
#include "tui/ScreenRefreshTimer.h"
#include "util/DirectoryWatcher.h"

class App {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    int run();

private:
    void applyConfig();
    static int clampVisual(IConfigService& config);

    NotificationBus bus_;
    ConfigService config_;
    MusicLibrary library_;
    CommandQueue cmdQueue_;
    PlaybackController controller_;
    LibraryService service_;
    AsyncLibraryService asyncService_;

    std::shared_ptr<std::atomic<bool>> reloadFlag_;
    std::shared_ptr<int> visualIndex_;

    ftxui::App screen_;
    ftxui::Component component_;
    ScreenRefreshTimer refreshTimer_;
    DirectoryWatcher dirWatcher_;
};

#endif
