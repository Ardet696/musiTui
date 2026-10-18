#pragma once

// Non-blocking pipe used to wake a poll() loop from another thread or from a
// signal handler (notify() is async-signal-safe).
class SelfPipe {
public:
    SelfPipe();
    ~SelfPipe();

    SelfPipe(const SelfPipe&) = delete;
    SelfPipe& operator=(const SelfPipe&) = delete;

    bool valid() const { return fds_[0] >= 0; }
    int readFd() const { return fds_[0]; }
    void notify() const noexcept;
    void drain() const noexcept;

private:
    int fds_[2] = {-1, -1};
};
