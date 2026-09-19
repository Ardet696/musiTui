#ifndef MP3PLAYER_AUDIODECODERFACTORY_H
#define MP3PLAYER_AUDIODECODERFACTORY_H

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

class IAudioDecoder;

class AudioDecoderFactory {
public:
    static std::span<const std::string_view> supportedExtensions();

    static bool isSupported(const std::filesystem::path& filePath);

    static std::unique_ptr<IAudioDecoder> create(const std::filesystem::path& filePath);
};

#endif // MP3PLAYER_AUDIODECODERFACTORY_H
