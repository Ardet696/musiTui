#include <catch2/catch_all.hpp>
#include "../../src/decode/WavDecoder.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

static std::filesystem::path fixtureDir() {
    return std::filesystem::path(FIXTURE_DIR);
}

static std::size_t drainFrames(WavDecoder& dec, std::int64_t* outSumAbs = nullptr) {
    std::vector<int16_t> buf(1152 * dec.channels());
    std::size_t total = 0;
    std::size_t frames;
    do {
        frames = dec.decodeFrames(buf, 1152);
        if (outSumAbs) {
            for (std::size_t i = 0; i < frames * dec.channels(); ++i)
                *outSumAbs += std::abs(buf[i]);
        }
        total += frames;
    } while (frames > 0);
    return total;
}

TEST_CASE("WavDecoder opens 16-bit stereo WAV", "[decoder][wav]") {
    WavDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.wav"));
    CHECK(dec.isOpen());
    CHECK(dec.sampleRate() == 44100);
    CHECK(dec.channels() == 2);
    CHECK(dec.totalSamples() == 44100u * 2u);
}

TEST_CASE("WavDecoder decodes the whole stream", "[decoder][wav]") {
    WavDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.wav"));
    CHECK(drainFrames(dec) == 44100u);
}

TEST_CASE("WavDecoder converts 8-bit mono to int16", "[decoder][wav]") {
    WavDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "silence_1s.wav"));
    CHECK(dec.channels() == 1);

    std::int64_t sumAbs = 0;
    CHECK(drainFrames(dec, &sumAbs) == 44100u);
    CHECK(sumAbs == 0);
}

TEST_CASE("WavDecoder converts 32-bit float to int16", "[decoder][wav]") {
    const auto readAll = [](const std::filesystem::path& p) {
        WavDecoder dec;
        REQUIRE(dec.open(p));
        std::vector<int16_t> out;
        std::vector<int16_t> buf(1152 * dec.channels());
        std::size_t frames;
        do {
            frames = dec.decodeFrames(buf, 1152);
            out.insert(out.end(), buf.begin(), buf.begin() + frames * dec.channels());
        } while (frames > 0);
        return out;
    };

    const auto reference = readAll(fixtureDir() / "sine_440hz_1s.wav");
    const auto converted = readAll(fixtureDir() / "sine_440hz_1s_f32.wav");

    REQUIRE(converted.size() == reference.size());
    REQUIRE(!reference.empty());

    int worst = 0;
    std::int64_t sumAbs = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        worst = std::max(worst, std::abs(converted[i] - reference[i]));
        sumAbs += std::abs(reference[i]);
    }

    CHECK(sumAbs > 0);
    CHECK(worst <= 1);
}

TEST_CASE("WavDecoder fails gracefully on nonexistent file", "[decoder][wav]") {
    WavDecoder dec;
    CHECK_FALSE(dec.open("/nonexistent/file.wav"));
    CHECK_FALSE(dec.isOpen());
    CHECK_FALSE(dec.lastError().empty());
}

TEST_CASE("WavDecoder rejects a file that is not WAV", "[decoder][wav]") {
    WavDecoder dec;
    CHECK_FALSE(dec.open(fixtureDir() / "sine_440hz_1s.flac"));
    CHECK_FALSE(dec.isOpen());
}

TEST_CASE("WavDecoder close and reopen", "[decoder][wav]") {
    WavDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "silence_1s.wav"));
    dec.close();
    CHECK_FALSE(dec.isOpen());
    CHECK(dec.sampleRate() == 0);

    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.wav"));
    CHECK(dec.sampleRate() == 44100);
    CHECK(dec.channels() == 2);
}

TEST_CASE("WavDecoder move transfers ownership", "[decoder][wav][raii]") {
    WavDecoder src;
    REQUIRE(src.open(fixtureDir() / "silence_1s.wav"));

    WavDecoder dst = std::move(src);
    CHECK(dst.isOpen());
    CHECK(dst.sampleRate() == 44100);
}

TEST_CASE("WavDecoder rejects truncated input without crashing", "[decoder][wav]") {
    const auto tmp = std::filesystem::temp_directory_path() / "musitui_truncated.wav";
    {
        std::ifstream in(fixtureDir() / "sine_440hz_1s.wav", std::ios::binary);
        std::vector<char> head(64);
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        std::ofstream out(tmp, std::ios::binary);
        out.write(head.data(), in.gcount());
    }

    WavDecoder dec;
    if (dec.open(tmp)) {
        drainFrames(dec);
        dec.close();
    }
    std::filesystem::remove(tmp);
    SUCCEED();
}
