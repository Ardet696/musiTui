#include "FlacDecoder.h"

#include <cassert>

#include "../config/Config.h"

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/dr_flac/dr_flac.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

struct FlacDecoder::Impl {
    drflac* dec = nullptr;
    int hz = 0;
    int ch = 0;
    std::string err;

    ~Impl() {
        if (dec) drflac_close(dec);
    }
};

FlacDecoder::FlacDecoder() : impl_(std::make_unique<Impl>()) {}
FlacDecoder::~FlacDecoder() = default;

FlacDecoder::FlacDecoder(FlacDecoder&&) noexcept = default;
FlacDecoder& FlacDecoder::operator=(FlacDecoder&&) noexcept = default;

bool FlacDecoder::open(const std::filesystem::path& filePath) {
    if (!impl_) return false;

    close();
    impl_->err.clear();

    const std::string pathStr = filePath.string();
    impl_->dec = drflac_open_file(pathStr.c_str(), nullptr);

    if (!impl_->dec) {
        impl_->err = "drflac_open_file failed";
        return false;
    }

    impl_->hz = static_cast<int>(impl_->dec->sampleRate);
    impl_->ch = static_cast<int>(impl_->dec->channels);

    if (impl_->hz <= 0 || impl_->hz > static_cast<int>(Config::MAX_SAMPLE_RATE)) {
        impl_->err = "Unsupported FLAC sample rate: " + std::to_string(impl_->hz) + " Hz";
        close();
        return false;
    }

    if (impl_->ch != 1 && impl_->ch != 2) {
        impl_->err = "Unsupported FLAC channel count: " + std::to_string(impl_->ch);
        close();
        return false;
    }

    return true;
}

void FlacDecoder::close() {
    if (!impl_) return;

    if (impl_->dec) {
        drflac_close(impl_->dec);
        impl_->dec = nullptr;
    }

    impl_->hz = 0;
    impl_->ch = 0;
}

bool FlacDecoder::isOpen() const {
    return impl_ && impl_->dec != nullptr;
}

int FlacDecoder::sampleRate() const {
    return impl_ ? impl_->hz : 0;
}

int FlacDecoder::channels() const {
    return impl_ ? impl_->ch : 0;
}

std::uint64_t FlacDecoder::totalSamples() const {
    if (!isOpen()) return 0;
    return impl_->dec->totalPCMFrameCount * static_cast<std::uint64_t>(impl_->ch);
}

std::size_t FlacDecoder::decodeFrames(std::span<int16_t> outInterleaved, std::size_t outFrames) {
    if (!isOpen() || outFrames == 0) return 0;

    assert(outInterleaved.size() >= outFrames * static_cast<std::size_t>(impl_->ch));

    const drflac_uint64 framesRead = drflac_read_pcm_frames_s16(
        impl_->dec, static_cast<drflac_uint64>(outFrames), outInterleaved.data());

    return static_cast<std::size_t>(framesRead);
}

const std::string& FlacDecoder::lastError() const {
    static const std::string kEmpty;
    return impl_ ? impl_->err : kEmpty;
}
