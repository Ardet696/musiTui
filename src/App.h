#ifndef MP3PLAYER_APP_H
#define MP3PLAYER_APP_H

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>

#include "PlayerCore.h"
#include "commands/CommandDispatcher.h"
#include "commands/IUiBridge.h"
#include "ipc/ControlServer.h"
#include "tui/ScreenRefreshTimer.h"
#include "util/DirectoryWatcher.h"

class App : private IUiBridge {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    int run();

private:
    void applyTheme(int index) override;
    void applyVisual(int index) override;
    void libraryChanged() override;
    void requestQuit() override;

    // Runs a socket request on the UI thread and waits for its response.
    std::string handleRemote(std::string_view request);

    static int clampVisual(IConfigService& config);

    PlayerCore core_;

    std::shared_ptr<std::atomic<bool>> reloadFlag_;
    std::shared_ptr<int> visualIndex_;

    ftxui::App screen_;
    CommandDispatcher dispatcher_;
    ftxui::Component component_;
    ScreenRefreshTimer refreshTimer_;
    DirectoryWatcher dirWatcher_;
    // Last, so it stops before anything its handler touches is destroyed.
    ControlServer controlServer_;
};

#endif
