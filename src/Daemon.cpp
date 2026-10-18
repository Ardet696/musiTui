#include "Daemon.h"

#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <poll.h>

namespace {

const SelfPipe* g_stopPipe = nullptr;

extern "C" void onStopSignal(int) {
    if (g_stopPipe) g_stopPipe->notify();
}

} // namespace

Daemon::Daemon()
    : dispatcher_(core_.service(), core_.config(), *this)
    , dirWatcher_(core_.config(), [this](const std::string& root) {
        std::lock_guard lock(dispatchMutex_);
        std::string error;
        core_.directService().setRootPath(root, error);
    })
    , controlServer_(resolveSocketPath(), [this](std::string_view request) {
        std::lock_guard lock(dispatchMutex_);
        return dispatcher_.execute(request).toJson();
    }) {
    g_stopPipe = &stop_;

    struct sigaction sa {};
    sa.sa_handler = onStopSignal;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
}

Daemon::~Daemon() {
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    g_stopPipe = nullptr;
}

int Daemon::run() {
    if (!controlServer_.listening()) {
        std::cerr << "musiTui: cannot start daemon: " << controlServer_.error() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "musiTui daemon listening on " << controlServer_.path().string() << std::endl;

    pollfd pfd{stop_.readFd(), POLLIN, 0};
    while (true) {
        int ready = ::poll(&pfd, 1, 500);
        drainNotifications();
        if (ready > 0) break;
        if (ready < 0 && errno != EINTR) break;
    }

    std::cout << "musiTui daemon stopped" << std::endl;
    return EXIT_SUCCESS;
}

void Daemon::drainNotifications() {
    for (const auto& n : core_.bus().drain()) {
        std::cerr << (n.level == NotifyLevel::Error ? "error: " : "") << n.message << '\n';
    }
}
