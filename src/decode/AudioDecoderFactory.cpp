#include "AudioDecoderFactory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

#include "FlacDecoder.h"
#include "IAudioDecoder.h"
#include "Mp3Decoder.h"
#include "WavDecoder.h"

namespace {

using DecoderMaker = std::unique_ptr<IAudioDecoder> (*)();

struct Entry {
    std::string_view extension;
    DecoderMaker make;
};

constexpr std::array<Entry, 3> kRegistry{{
    {".mp3",  []() -> std::unique_ptr<IAudioDecoder> { return std::make_unique<Mp3Decoder>(); }},
    {".flac", []() -> std::unique_ptr<IAudioDecoder> { return std::make_unique<FlacDecoder>(); }},
    {".wav",  []() -> std::unique_ptr<IAudioDecoder> { return std::make_unique<WavDecoder>(); }},
}};

std::string lowerExtension(const std::filesystem::path& filePath) {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

const Entry* findEntry(const std::filesystem::path& filePath) {
    const std::string ext = lowerExtension(filePath);
    const auto it = std::find_if(kRegistry.begin(), kRegistry.end(),
                                 [&ext](const Entry& e) { return e.extension == ext; });
    return it == kRegistry.end() ? nullptr : &*it;
}

}

std::span<const std::string_view> AudioDecoderFactory::supportedExtensions() {
    static const std::array<std::string_view, kRegistry.size()> kExtensions = [] {
        std::array<std::string_view, kRegistry.size()> out{};
        std::transform(kRegistry.begin(), kRegistry.end(), out.begin(),
                       [](const Entry& e) { return e.extension; });
        return out;
    }();
    return kExtensions;
}

bool AudioDecoderFactory::isSupported(const std::filesystem::path& filePath) {
    return findEntry(filePath) != nullptr;
}

std::unique_ptr<IAudioDecoder> AudioDecoderFactory::create(const std::filesystem::path& filePath) {
    const Entry* entry = findEntry(filePath);
    return entry ? entry->make() : nullptr;
}
