#pragma once

#include <filesystem>
#include <string>

struct SocketLocation {
    std::filesystem::path path;
    bool privateDir = false;  // parent is a per-user dir we own and must verify
};

/**
 * Where the control socket lives, in priority order:
 *   $MUSITUI_SOCKET
 *   $XDG_RUNTIME_DIR/musitui.sock
 *   ${TMPDIR:-/tmp}/musitui-<uid>/musitui.sock
 */
SocketLocation resolveSocketPath();

// Checks that dir is a real directory owned by us with no group/other access.
// With create=true a missing dir is created with mode 0700 first.
bool ensurePrivateDir(const std::filesystem::path& dir, bool create, std::string& error);
