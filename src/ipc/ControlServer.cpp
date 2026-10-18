#include "ControlServer.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "SocketIo.h"
#include "../util/Json.h"

namespace {

constexpr auto kClientTimeout = std::chrono::milliseconds(3000);

bool peerIsSameUser(int fd) {
#if defined(SO_PEERCRED)
    ucred cred{};
    socklen_t len = sizeof(cred);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) return false;
    return cred.uid == ::geteuid();
#else
    uid_t uid = 0;
    gid_t gid = 0;
    if (::getpeereid(fd, &uid, &gid) != 0) return false;
    return uid == ::geteuid();
#endif
}

std::string errorResponse(std::string_view message) {
    return "{\"ok\":false,\"message\":" + json::quote(message) + "}\n";
}

} // namespace

ControlServer::ControlServer(SocketLocation location, Handler handler)
    : location_(std::move(location)), handler_(std::move(handler))
{
    if (!wake_.valid()) {
        error_ = "cannot create wake pipe";
        return;
    }
    if (bindSocket()) {
        thread_ = std::jthread([this](std::stop_token st) { run(st); });
    }
}

ControlServer::~ControlServer() {
    if (thread_.joinable()) {
        thread_.request_stop();
        wake_.notify();
        thread_.join();
    }
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        ::unlink(location_.path.c_str());
    }
}

bool ControlServer::bindSocket() {
    if (location_.privateDir && !ensurePrivateDir(location_.path.parent_path(), true, error_)) {
        return false;
    }

    sockaddr_un addr{};
    if (!sockio::makeAddress(location_.path, addr, error_)) return false;

    struct stat st {};
    if (::lstat(location_.path.c_str(), &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            error_ = location_.path.string() + " exists and is not a socket";
            return false;
        }
        // A socket that still accepts belongs to a live instance; otherwise it
        // was left behind by a crash and is safe to replace.
        int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
        bool alive = probe >= 0 &&
                     ::connect(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
        if (probe >= 0) ::close(probe);
        if (alive) {
            error_ = "another musiTui instance owns " + location_.path.string();
            return false;
        }
        ::unlink(location_.path.c_str());
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        error_ = std::string("socket: ") + std::strerror(errno);
        return false;
    }
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::chmod(location_.path.c_str(), 0600) != 0 ||
        ::listen(fd, 8) != 0) {
        error_ = location_.path.string() + ": " + std::strerror(errno);
        ::close(fd);
        return false;
    }

    listenFd_ = fd;
    return true;
}

void ControlServer::run(std::stop_token stop) {
    pollfd fds[2] = {
        {listenFd_, POLLIN, 0},
        {wake_.readFd(), POLLIN, 0},
    };

    while (!stop.stop_requested()) {
        if (::poll(fds, 2, -1) < 0) {
            if (errno == EINTR) continue;
            return;
        }
        if (fds[1].revents) {
            wake_.drain();
            continue;
        }
        if (!(fds[0].revents & POLLIN)) continue;

        int client = ::accept(listenFd_, nullptr, nullptr);
        if (client < 0) continue;

        // BSD/macOS accept() inherits O_NONBLOCK from the listener; Linux does not.
        ::fcntl(client, F_SETFL, ::fcntl(client, F_GETFL) & ~O_NONBLOCK);
        ::fcntl(client, F_SETFD, FD_CLOEXEC);
        serve(client);
        ::close(client);
    }
}

void ControlServer::serve(int clientFd) {
    sockio::configureStream(clientFd, kClientTimeout);
    if (!peerIsSameUser(clientFd)) return;

    std::string request;
    switch (sockio::readLine(clientFd, request)) {
        case sockio::ReadStatus::Error:
            return;
        case sockio::ReadStatus::TooLong:
            sockio::sendAll(clientFd, errorResponse("Request too long"));
            return;
        case sockio::ReadStatus::Line:
            break;
    }

    std::string response;
    try {
        response = handler_(request);
        response.push_back('\n');
    } catch (const std::exception& e) {
        response = errorResponse(std::string("Internal error: ") + e.what());
    }
    sockio::sendAll(clientFd, response);
}
