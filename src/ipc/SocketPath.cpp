#include "SocketPath.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace {

const char* nonEmptyEnv(const char* name) {
    const char* value = std::getenv(name);
    return (value && *value) ? value : nullptr;
}

} // namespace

SocketLocation resolveSocketPath() {
    if (const char* custom = nonEmptyEnv("MUSITUI_SOCKET")) return {custom, false};
    if (const char* runtime = nonEmptyEnv("XDG_RUNTIME_DIR")) {
        return {std::filesystem::path(runtime) / "musitui.sock", false};
    }

    const char* tmp = nonEmptyEnv("TMPDIR");
    std::filesystem::path base = tmp ? tmp : "/tmp";
    return {base / ("musitui-" + std::to_string(::geteuid())) / "musitui.sock", true};
}

bool ensurePrivateDir(const std::filesystem::path& dir, bool create, std::string& error) {
    if (create && ::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
        error = "cannot create " + dir.string() + ": " + std::strerror(errno);
        return false;
    }

    struct stat st {};
    if (::lstat(dir.c_str(), &st) != 0) {
        error = dir.string() + ": " + std::strerror(errno);
        return false;
    }
    if (!S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() || (st.st_mode & 077) != 0) {
        error = dir.string() + " is not a private directory owned by this user";
        return false;
    }
    return true;
}
