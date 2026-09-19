#include "SongQueue.h"
#include "../decode/IAudioDecoder.h"
#include <vector>
#include <span>

#include "../events/NotificationBus.h"

SongQueue::SongQueue(NotificationBus* bus)
    : currentIndex_(-1)
    , preWarmedUpTo_(-1)
    , bus_(bus)
{
}

SongQueue::~SongQueue() {
    // jthread automatically requests stop and joins
    clear();
}

void SongQueue::loadPlaylist(const std::vector<std::filesystem::path>& songPaths) {
    // Stop pre-warm thread BEFORE acquiring mutex to avoid deadlock
    // (preWarmLoop also locks mutex_)
    if (preWarmThread_.joinable()) {
        preWarmThread_.request_stop();
        preWarmThread_ = std::jthread();
    }

    std::lock_guard<std::mutex> lock(mutex_);

    playlist_ = songPaths;
    currentIndex_ = playlist_.empty() ? -1 : 0;
    preWarmedUpTo_.store(-1);

    if (!playlist_.empty()) {
        preWarmThread_ = std::jthread([this](std::stop_token stopToken) {
            preWarmLoop(stopToken);
        });
    }
}

std::filesystem::path SongQueue::getCurrentSongPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(playlist_.size())) {
        return {};
    }
    return playlist_[currentIndex_];
}

std::filesystem::path SongQueue::next() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (currentIndex_ < 0 || playlist_.empty()) {
        return {};
    }

    currentIndex_++;
    if (currentIndex_ >= static_cast<int>(playlist_.size())) {
        currentIndex_ = static_cast<int>(playlist_.size()) - 1;
        return {};
    }

    return playlist_[currentIndex_];
}

std::filesystem::path SongQueue::previous() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (currentIndex_ <= 0 || playlist_.empty()) {
        return {};
    }

    currentIndex_--;
    return playlist_[currentIndex_];
}

std::filesystem::path SongQueue::skipTo(int index) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (index < 0 || index >= static_cast<int>(playlist_.size())) {
        return {};
    }

    currentIndex_ = index;
    return playlist_[currentIndex_];
}

std::filesystem::path SongQueue::getNextSongPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int nextIndex = currentIndex_ + 1;
    if (nextIndex < 0 || nextIndex >= static_cast<int>(playlist_.size())) {
        return {};
    }
    return playlist_[nextIndex];
}

bool SongQueue::isNextSongReady() const {
    const int nextIndex = currentIndex_.load(std::memory_order_relaxed) + 1;
    return preWarmedUpTo_.load() >= nextIndex;
}

void SongQueue::clear() {
    // Stop pre-warm thread BEFORE acquiring mutex to avoid deadlock
    if (preWarmThread_.joinable()) {
        preWarmThread_.request_stop();
        preWarmThread_ = std::jthread();
    }

    std::lock_guard<std::mutex> lock(mutex_);

    playlist_.clear();
    currentIndex_ = -1;
    preWarmedUpTo_.store(-1);
}

void SongQueue::preWarmLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        int targetIndex = currentIndex_.load(std::memory_order_relaxed) + Config::PRE_WARM_AHEAD_COUNT;
        int warmedUpTo = preWarmedUpTo_.load();

        for (int i = warmedUpTo + 1; i <= targetIndex && i < static_cast<int>(playlist_.size()); ++i) {
            if (stopToken.stop_requested()) break;

            std::filesystem::path songPath;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                songPath = playlist_[i];
            }

            if (preWarmSong(songPath)) {
                preWarmedUpTo_.store(i);
            }
        }

        std::this_thread::sleep_for(Config::PRE_WARM_POLL_INTERVAL_MS);
    }
}

bool SongQueue::preWarmSong(const std::filesystem::path& songPath) {
    auto decoder = AudioDecoderFactory::create(songPath);
    if (!decoder || !decoder->open(songPath)) {
        if (bus_) bus_->push("Pre-warm failed: " + songPath.filename().string(), NotifyLevel::Error);
        return false;
    }

    const int channels = decoder->channels();

    constexpr std::size_t chunkFrames = Config::DECODE_CHUNK_FRAMES;
    std::vector<int16_t> buffer(chunkFrames * channels);

    for (int i = 0; i < Config::PRE_WARM_VALIDATION_FRAMES; ++i) {
        const std::size_t framesDecoded = decoder->decodeFrames(
            std::span(buffer.data(), buffer.size()),
            chunkFrames
        );
        if (framesDecoded == 0) break;
    }

    decoder->close();
    return true;
}
