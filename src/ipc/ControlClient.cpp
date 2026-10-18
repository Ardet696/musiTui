#include "ControlClient.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "SocketIo.h"
#include "../commands/CommandDispatcher.h"
#include "../util/Json.h"

namespace {

constexpr auto kResponseTimeout = std::chrono::milliseconds(5000);

} // namespace

std::optional<std::string> sendControlRequest(const SocketLocation& location,
                                              std::string_view request,
                                              std::string& error) {
    if (location.privateDir && !ensurePrivateDir(location.path.parent_path(), false, error)) {
        return std::nullopt;
    }

    sockaddr_un addr{};
    if (!sockio::makeAddress(location.path, addr, error)) return std::nullopt;

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::string("socket: ") + std::strerror(errno);
        return std::nullopt;
    }
    sockio::configureStream(fd, kResponseTimeout);

    std::optional<std::string> response;
    std::string line;
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = location.path.string() + ": " + std::strerror(errno);
    } else if (!sockio::sendAll(fd, std::string(request) + '\n')) {
        error = std::string("send: ") + std::strerror(errno);
    } else if (sockio::readLine(fd, line) != sockio::ReadStatus::Line || line.empty()) {
        error = "no response from player";
    } else {
        response = std::move(line);
    }
    ::close(fd);
    return response;
}

int runControlClient(std::span<const std::string_view> args) {
    bool rawJson = false;
    std::string request;
    for (auto arg : args) {
        if (arg == "--json") {
            rawJson = true;
            continue;
        }
        if (!request.empty()) request += ' ';
        request += CommandDispatcher::quoteArg(arg);
    }

    if (request.empty()) {
        std::cerr << "usage: musiTui ctl [--json] <command> [args...]\n"
                     "       musiTui ctl help    list commands\n";
        return ctl_exit::kRejected;
    }

    SocketLocation location = resolveSocketPath();
    std::string error;
    auto response = sendControlRequest(location, request, error);
    if (!response) {
        std::cerr << "musiTui is not running or not reachable (" << error << ").\n"
                     "Start it with `musiTui` in a terminal or `musiTui --daemon` in the background.\n";
        return ctl_exit::kUnreachable;
    }

    bool ok = response->starts_with("{\"ok\":true");
    if (rawJson) {
        std::cout << *response << '\n';
    } else {
        std::string message = json::findString(*response, "message").value_or(*response);
        (ok ? std::cout : std::cerr) << message << '\n';
    }
    return ok ? ctl_exit::kOk : ctl_exit::kRejected;
}
