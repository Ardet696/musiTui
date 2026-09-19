#include "Song.h"
#include "../decode/AudioDecoderFactory.h"
#include "../decode/IAudioDecoder.h"
#include "../config/Config.h"
#include <vector>
#include <sstream>
#include <iomanip>

Song::Song(const std::filesystem::path& filePath, const std::string& albumName)
    : filePath_(filePath) , title_(filePath.stem().string())  // Use filename without extension as title
    , artist_("Unknown")
     , album_(albumName)
      , type_(SongType::Standard)
     , durationSeconds_(0)
{
    extractMetadata();
}

void Song::extractMetadata() {

    if (!std::filesystem::exists(filePath_)) {
        durationSeconds_ = 0;
        type_ = SongType::Standard;
        return;
    }

    const std::uintmax_t fileSizeBytes = std::filesystem::file_size(filePath_);

    auto decoder = AudioDecoderFactory::create(filePath_);
    if (!decoder || !decoder->open(filePath_)) {
        durationSeconds_ = 0;
        type_ = SongType::Standard;
        return;
    }

    const int sampleRate = decoder->sampleRate();
    const int channels = decoder->channels();
    const std::uint64_t totalSamples = decoder->totalSamples();

    const std::size_t sampleFrames = 10;
    const std::size_t chunkFrames = 1152;
    std::vector<int16_t> buffer(chunkFrames * channels);

    std::size_t totalFramesSampled = 0;
    for (std::size_t i = 0; i < sampleFrames; ++i) {
        const std::size_t framesDecoded = decoder->decodeFrames(
            std::span<int16_t>(buffer.data(), buffer.size()),
            chunkFrames
        );
        if (framesDecoded == 0) break;
        totalFramesSampled += framesDecoded;
    }

    decoder->close();

    if (totalFramesSampled == 0 || sampleRate == 0 || channels == 0) {
        durationSeconds_ = 0;
        type_ = SongType::Standard;
        return;
    }

    if (totalSamples > 0) {
        durationSeconds_ = static_cast<int>(
            totalSamples / (static_cast<std::uint64_t>(sampleRate) * channels)
        );
    } else {
        durationSeconds_ = static_cast<int>(
            (fileSizeBytes * 8.0) / Config::ESTIMATED_MP3_BITRATE
        );
    }

    type_ = classifyByDuration(durationSeconds_);
}

Song::SongType Song::classifyByDuration(int durationSeconds) {
    if (durationSeconds < 120) {
        return SongType::Interlude;
    } else if (durationSeconds < 420) {
        return SongType::Standard;
    } else if (durationSeconds < 900) {
        return SongType::Suite;
    } else {
        return SongType::Medley;
    }
}

std::string Song::getTypeAsString() const {
    switch (type_) {
        case SongType::Interlude: return "Interlude";
        case SongType::Standard: return "Standard";
        case SongType::Suite: return "Suite";
        case SongType::Medley: return "Medley";
        default: return "Unknown";
    }
}

std::string Song::getFormattedDuration() const {
    const int minutes = durationSeconds_ / 60;
    const int seconds = durationSeconds_ % 60;

    std::ostringstream oss;
    oss << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}
