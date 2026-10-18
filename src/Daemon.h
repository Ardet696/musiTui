#ifndef MP3PLAYER_DAEMON_H
#define MP3PLAYER_DAEMON_H

#include <mutex>
#include <string>
#include <string_view>

#include "PlayerCore.h"
#include "commands/CommandDispatcher.h"
#include "commands/IUiBridge.h"
#include "ipc/ControlServer.h"
#include "util/DirectoryWatcher.h"
#include "util/SelfPipe.h"

/**
 * Daemon - headless player driven only through the control socket.
 * Notifications are logged to stderr; SIGINT, SIGTERM or `quit` stop it.
 */
class Daemon : private IUiBridge {
public:
    Daemon();
    ~Daemon() override;

    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;
    Daemon(Daemon&&) = delete;
    Daemon& operator=(Daemon&&) = delete;

    int run();

private:
    // Theme and visual are only persisted (config) until a TUI shows them.
    void applyTheme(int) override {}
    void applyVisual(int) override {}
    void libraryChanged() override {}
    void requestQuit() override { stop_.notify(); }

    void drainNotifications();

    SelfPipe stop_;
    PlayerCore core_;
    CommandDispatcher dispatcher_;
    // The control thread and the directory watcher both reach the service;
    // this keeps their requests strictly one at a time.
    std::mutex dispatchMutex_;
    DirectoryWatcher dirWatcher_;
    ControlServer controlServer_;
};

#endif
