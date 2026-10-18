#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "SocketPath.h"

// Exit codes of `musiTui ctl`, documented in AGENTS.md.
namespace ctl_exit {
constexpr int kOk = 0;
constexpr int kRejected = 1;     // the player answered with ok:false, or bad usage
constexpr int kUnreachable = 2;  // no running player to talk to
}

// Sends one request line and returns the raw response line.
std::optional<std::string> sendControlRequest(const SocketLocation& location,
                                              std::string_view request,
                                              std::string& error);

// Entry point for `musiTui ctl [--json] <command> [args...]`.
int runControlClient(std::span<const std::string_view> args);
