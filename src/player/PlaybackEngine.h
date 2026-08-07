#ifndef MP3PLAYER_PLAYBACKENGINE_H
#define MP3PLAYER_PLAYBACKENGINE_H

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../decode/SpectrumAnalyzer.h"
#include "../decode/BpmDetector.h"
#include "../util/AudioFormat.h"

class IAudioDecoder;
class IAudioSink;
class DecodeThread;
class NotificationBus;
template<typename T> class RingBuffer;

class PlaybackEngine {
public:
    enum class State {
        Stopped,
        Playing,
        Paused
    };

    using DecoderFactory = std::function<std::unique_ptr<IAudioDecoder>()>;
    using SinkFactory    = std::function<std::unique_ptr<IAudioSink>(NotificationBus*)>;

    explicit PlaybackEngine(NotificationBus* bus = nullptr,
                            DecoderFactory decoderFactory = {},
                            SinkFactory sinkFactory = {});
    ~PlaybackEngine();

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;
    PlaybackEngine(PlaybackEngine&&) = delete;
    PlaybackEngine& operator=(PlaybackEngine&&) = delete;

    bool load(const std::filesystem::path& mp3File);
    void play();
    void pause();
    void stop();

    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    State getState() const;


    bool hasReachedEndOfStream() const;

    std::filesystem::path getCurrentFile() const;
    int getSampleRate() const;
    int getChannels() const;

    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer_; }
    const SpectrumAnalyzer& getSpectrumAnalyzer() const { return spectrumAnalyzer_; }
    float getPlaybackProgress() const;
    float getBpmLoadFactor() const;

    void setVolume(int percent);  // 0-100
    int  getVolume() const;

    // Returns false and keeps the current (working) device if the requested one
    // cannot be opened. Never tears down active playback on failure.
    bool setOutputDevice(const std::string& deviceName);
    std::string getOutputDevice() const;

    static std::vector<std::string> listOutputDevices();

private:
    using FrameProvider = std::function<std::size_t(int16_t*, std::size_t)>;

    bool startPlayback();
    void stopPlayback();
    FrameProvider makeAudioProvider();
    std::string deviceLabel(const std::string& name) const;

    // Opens the device on first use, then only re-points it at the new source.
    bool prepareSink(const AudioFormat& fmt);
    bool openSink(const AudioFormat& fmt);

    DecoderFactory decoderFactory_;
    SinkFactory sinkFactory_;

    // Playback components (owned by PlaybackEngine)
    std::unique_ptr<IAudioDecoder> decoder_;
    std::unique_ptr<IAudioSink> sink_;
    std::unique_ptr<RingBuffer<int16_t>> ringBuffer_;
    std::unique_ptr<DecodeThread> decodeThread_;

    std::atomic<State> state_;
    std::filesystem::path currentFile_;
    std::atomic<int> sampleRate_;
    std::atomic<int> channels_;
    std::atomic<int> silentCallbacks_;
    SpectrumAnalyzer spectrumAnalyzer_;
    BpmDetector bpmDetector_;
    std::atomic<std::uint64_t> samplesPlayed_;
    std::atomic<std::uint64_t> totalSamples_;
    std::string outputDeviceName_;
    std::atomic<int> volume_{100};

    NotificationBus* bus_;
};

#endif
