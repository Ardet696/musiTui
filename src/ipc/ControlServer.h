#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <thread>

#include "SocketPath.h"
#include "../util/SelfPipe.h"

/**
 * ControlServer - local Unix socket that lets scripts and agents drive the
 * running player (see AGENTS.md for the protocol).
 *
 * One connection is served at a time on a dedicated thread: read one request
 * line, pass it to the handler, write back its one-line response, close.
 * Only the owning user can connect (0600 socket + peer uid check).
 */
class ControlServer {
public:
    // Receives one request line, returns the response line without '\n'.
    using Handler = std::function<std::string(std::string_view request)>;

    ControlServer(SocketLocation location, Handler handler);
    ~ControlServer();

    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;
    ControlServer(ControlServer&&) = delete;
    ControlServer& operator=(ControlServer&&) = delete;

    bool listening() const { return listenFd_ >= 0; }
    const std::string& error() const { return error_; }
    const std::filesystem::path& path() const { return location_.path; }

private:
    bool bindSocket();
    void run(std::stop_token stop);
    void serve(int clientFd);

    SocketLocation location_;
    Handler handler_;
    SelfPipe wake_;
    int listenFd_ = -1;
    std::string error_;
    std::jthread thread_;
};
