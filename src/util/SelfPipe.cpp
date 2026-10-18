#include "SelfPipe.h"

#include <fcntl.h>
#include <unistd.h>

SelfPipe::SelfPipe() {
    if (::pipe(fds_) != 0) {
        fds_[0] = fds_[1] = -1;
        return;
    }
    for (int fd : fds_) {
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
}

SelfPipe::~SelfPipe() {
    for (int fd : fds_) {
        if (fd >= 0) ::close(fd);
    }
}

void SelfPipe::notify() const noexcept {
    if (fds_[1] < 0) return;
    char byte = 1;
    // A full pipe already guarantees a pending wake-up, so EAGAIN is fine.
    [[maybe_unused]] auto n = ::write(fds_[1], &byte, 1);
}

void SelfPipe::drain() const noexcept {
    char buf[64];
    while (::read(fds_[0], buf, sizeof(buf)) > 0) {}
}
