#include "SocketIo.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace sockio {

bool makeAddress(const std::filesystem::path& path, sockaddr_un& addr, std::string& error) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    const std::string& native = path.native();
    if (native.empty() || native.size() >= sizeof(addr.sun_path)) {
        error = "socket path too long: " + native;
        return false;
    }
    std::memcpy(addr.sun_path, native.c_str(), native.size() + 1);
    return true;
}

void configureStream(int fd, std::chrono::milliseconds timeout) {
    timeval tv{};
    tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout.count() / 1000);
    tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout.count() % 1000) * 1000);
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#ifdef SO_NOSIGPIPE
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
}

bool sendAll(int fd, std::string_view data) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    while (!data.empty()) {
        ssize_t n = ::send(fd, data.data(), data.size(), flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

ReadStatus readLine(int fd, std::string& out) {
    out.clear();
    char buf[512];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return ReadStatus::Error;
        }
        if (n == 0) return ReadStatus::Line;

        std::string_view chunk(buf, static_cast<std::size_t>(n));
        auto newline = chunk.find('\n');
        out.append(chunk.substr(0, newline));
        if (out.size() > kMaxLine) return ReadStatus::TooLong;
        if (newline != std::string_view::npos) return ReadStatus::Line;
    }
}

} // namespace sockio
