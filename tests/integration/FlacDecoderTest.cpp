#include <catch2/catch_all.hpp>
#include "../../src/decode/FlacDecoder.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

static std::filesystem::path fixtureDir() {
    return std::filesystem::path(FIXTURE_DIR);
}

TEST_CASE("FlacDecoder opens valid FLAC", "[decoder][flac]") {
    FlacDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.flac"));
    CHECK(dec.isOpen());
    CHECK(dec.sampleRate() == 44100);
    CHECK(dec.channels() == 2);
    CHECK(dec.totalSamples() == 44100u * 2u);
}

TEST_CASE("FlacDecoder decodes the whole stream", "[decoder][flac]") {
    FlacDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.flac"));

    std::vector<int16_t> buf(1152 * dec.channels());
    std::size_t totalFrames = 0;
    std::size_t frames;

    do {
        frames = dec.decodeFrames(buf, 1152);
        totalFrames += frames;
    } while (frames > 0);

    CHECK(totalFrames == 44100u);
}

TEST_CASE("FlacDecoder silence file decodes to zero", "[decoder][flac]") {
    FlacDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "silence_1s.flac"));

    std::vector<int16_t> buf(1152 * dec.channels());
    std::int64_t sumAbs = 0;
    std::size_t frames;

    do {
        frames = dec.decodeFrames(buf, 1152);
        for (std::size_t i = 0; i < frames * dec.channels(); ++i)
            sumAbs += std::abs(buf[i]);
    } while (frames > 0);

    CHECK(sumAbs == 0);
}

TEST_CASE("FlacDecoder fails gracefully on nonexistent file", "[decoder][flac]") {
    FlacDecoder dec;
    CHECK_FALSE(dec.open("/nonexistent/file.flac"));
    CHECK_FALSE(dec.isOpen());
    CHECK_FALSE(dec.lastError().empty());
}

TEST_CASE("FlacDecoder rejects a file that is not FLAC", "[decoder][flac]") {
    FlacDecoder dec;
    CHECK_FALSE(dec.open(fixtureDir() / "sine_440hz_1s.mp3"));
    CHECK_FALSE(dec.isOpen());
}

TEST_CASE("FlacDecoder close and reopen", "[decoder][flac]") {
    FlacDecoder dec;
    REQUIRE(dec.open(fixtureDir() / "silence_1s.flac"));
    dec.close();
    CHECK_FALSE(dec.isOpen());
    CHECK(dec.sampleRate() == 0);

    REQUIRE(dec.open(fixtureDir() / "sine_440hz_1s.flac"));
    CHECK(dec.sampleRate() == 44100);
}

TEST_CASE("FlacDecoder move transfers ownership", "[decoder][flac][raii]") {
    FlacDecoder src;
    REQUIRE(src.open(fixtureDir() / "silence_1s.flac"));

    FlacDecoder dst = std::move(src);
    CHECK(dst.isOpen());
    CHECK(dst.sampleRate() == 44100);
}

TEST_CASE("FlacDecoder rejects truncated input without crashing", "[decoder][flac]") {
    const auto tmp = std::filesystem::temp_directory_path() / "musitui_truncated.flac";
    {
        std::ifstream in(fixtureDir() / "sine_440hz_1s.flac", std::ios::binary);
        std::vector<char> head(64);
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        std::ofstream out(tmp, std::ios::binary);
        out.write(head.data(), in.gcount());
    }

    FlacDecoder dec;
    if (dec.open(tmp)) {
        std::vector<int16_t> buf(1152 * dec.channels());
        while (dec.decodeFrames(buf, 1152) > 0) {}
        dec.close();
    }
    std::filesystem::remove(tmp);
    SUCCEED();
}
