#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

struct sockaddr_un;

// Low-level helpers shared by the control server and client. Wire format is
// one request line in, one JSON line out, then the server closes.
namespace sockio {

constexpr std::size_t kMaxLine = 4096;

bool makeAddress(const std::filesystem::path& path, sockaddr_un& addr, std::string& error);

// Applies send/receive timeouts and disables SIGPIPE where the platform needs
// a socket option for it (macOS). Linux uses MSG_NOSIGNAL in sendAll.
void configureStream(int fd, std::chrono::milliseconds timeout);

bool sendAll(int fd, std::string_view data);

enum class ReadStatus { Line, TooLong, Error };

// Reads up to the first '\n' (excluded) or EOF. Fails with TooLong past kMaxLine.
ReadStatus readLine(int fd, std::string& out);

} // namespace sockio
