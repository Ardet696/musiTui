#include <catch2/catch_all.hpp>
#include "../../src/decode/AudioDecoderFactory.h"
#include "../../src/decode/IAudioDecoder.h"
#include <algorithm>

TEST_CASE("Factory maps known extensions to a decoder", "[decoder][factory]") {
    CHECK(AudioDecoderFactory::create("song.mp3") != nullptr);
    CHECK(AudioDecoderFactory::create("song.flac") != nullptr);
    CHECK(AudioDecoderFactory::create("song.wav") != nullptr);
}

TEST_CASE("Factory extension matching is case insensitive", "[decoder][factory]") {
    CHECK(AudioDecoderFactory::isSupported("SONG.FLAC"));
    CHECK(AudioDecoderFactory::isSupported("song.Mp3"));
    CHECK(AudioDecoderFactory::isSupported("song.WaV"));
    CHECK(AudioDecoderFactory::create("SONG.FLAC") != nullptr);
}

TEST_CASE("Factory rejects unknown and missing extensions", "[decoder][factory]") {
    CHECK_FALSE(AudioDecoderFactory::isSupported("cover.jpg"));
    CHECK_FALSE(AudioDecoderFactory::isSupported("README"));
    CHECK_FALSE(AudioDecoderFactory::isSupported("album.flac.txt"));
    CHECK(AudioDecoderFactory::create("cover.jpg") == nullptr);
}

TEST_CASE("Supported extension list is lowercase and dot prefixed", "[decoder][factory]") {
    const auto exts = AudioDecoderFactory::supportedExtensions();
    REQUIRE(!exts.empty());
    for (const auto ext : exts) {
        CHECK(ext.front() == '.');
        CHECK(std::none_of(ext.begin(), ext.end(),
                           [](char c) { return std::isupper(static_cast<unsigned char>(c)); }));
        CHECK(AudioDecoderFactory::isSupported("track" + std::string(ext)));
    }
}

TEST_CASE("Freshly created decoder is closed", "[decoder][factory]") {
    auto dec = AudioDecoderFactory::create("song.flac");
    REQUIRE(dec);
    CHECK_FALSE(dec->isOpen());
    CHECK(dec->sampleRate() == 0);
    CHECK(dec->channels() == 0);
}
