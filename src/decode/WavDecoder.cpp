#include "WavDecoder.h"

#include <cassert>

#include "../config/Config.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_WCHAR

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/dr_wav/dr_wav.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

struct WavDecoder::Impl {
    drwav dec{};
    bool open = false;
    int hz = 0;
    int ch = 0;
    std::string err;

    ~Impl() {
        if (open) drwav_uninit(&dec);
    }
};

WavDecoder::WavDecoder() : impl_(std::make_unique<Impl>()) {}
WavDecoder::~WavDecoder() = default;

WavDecoder::WavDecoder(WavDecoder&&) noexcept = default;
WavDecoder& WavDecoder::operator=(WavDecoder&&) noexcept = default;

bool WavDecoder::open(const std::filesystem::path& filePath) {
    if (!impl_) return false;

    close();
    impl_->err.clear();

    const std::string pathStr = filePath.string();
    if (!drwav_init_file(&impl_->dec, pathStr.c_str(), nullptr)) {
        impl_->err = "drwav_init_file failed";
        return false;
    }
    impl_->open = true;

    impl_->hz = static_cast<int>(impl_->dec.sampleRate);
    impl_->ch = static_cast<int>(impl_->dec.channels);

    if (impl_->hz <= 0 || impl_->hz > static_cast<int>(Config::MAX_SAMPLE_RATE)) {
        impl_->err = "Unsupported WAV sample rate: " + std::to_string(impl_->hz) + " Hz";
        close();
        return false;
    }

    if (impl_->ch != 1 && impl_->ch != 2) {
        impl_->err = "Unsupported WAV channel count: " + std::to_string(impl_->ch);
        close();
        return false;
    }

    return true;
}

void WavDecoder::close() {
    if (!impl_) return;

    if (impl_->open) {
        drwav_uninit(&impl_->dec);
        impl_->open = false;
    }

    impl_->hz = 0;
    impl_->ch = 0;
}

bool WavDecoder::isOpen() const {
    return impl_ && impl_->open;
}

int WavDecoder::sampleRate() const {
    return impl_ ? impl_->hz : 0;
}

int WavDecoder::channels() const {
    return impl_ ? impl_->ch : 0;
}

std::uint64_t WavDecoder::totalSamples() const {
    if (!isOpen()) return 0;
    return impl_->dec.totalPCMFrameCount * static_cast<std::uint64_t>(impl_->ch);
}

std::size_t WavDecoder::decodeFrames(std::span<int16_t> outInterleaved, std::size_t outFrames) {
    if (!isOpen() || outFrames == 0) return 0;

    assert(outInterleaved.size() >= outFrames * static_cast<std::size_t>(impl_->ch));

    const drwav_uint64 framesRead = drwav_read_pcm_frames_s16(
        &impl_->dec, static_cast<drwav_uint64>(outFrames), outInterleaved.data());

    return static_cast<std::size_t>(framesRead);
}

const std::string& WavDecoder::lastError() const {
    static const std::string kEmpty;
    return impl_ ? impl_->err : kEmpty;
}
