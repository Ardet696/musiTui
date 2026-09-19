#ifndef MP3PLAYER_WAVDECODER_H
#define MP3PLAYER_WAVDECODER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "IAudioDecoder.h"

class WavDecoder : public IAudioDecoder {
public:
    WavDecoder();
    ~WavDecoder() override;

    WavDecoder(const WavDecoder&) = delete;
    WavDecoder& operator=(const WavDecoder&) = delete;
    WavDecoder(WavDecoder&&) noexcept;
    WavDecoder& operator=(WavDecoder&&) noexcept;

    bool open(const std::filesystem::path& filePath) override;
    void close() override;

    bool isOpen() const override;
    int sampleRate() const override;
    int channels() const override;
    std::uint64_t totalSamples() const override;

    std::size_t decodeFrames(std::span<int16_t> outInterleaved, std::size_t outFrames) override;

    const std::string& lastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // MP3PLAYER_WAVDECODER_H
