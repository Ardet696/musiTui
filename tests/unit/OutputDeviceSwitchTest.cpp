#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../../src/audio/IAudioSink.h"
#include "../../src/decode/IAudioDecoder.h"
#include "../../src/events/NotificationBus.h"
#include "../../src/player/PlaybackEngine.h"

namespace {

class FakeDecoder final : public IAudioDecoder {
public:
    bool open(const std::filesystem::path&) override { open_ = true; return true; }
    void close() override { open_ = false; }
    bool isOpen() const override { return open_; }
    int sampleRate() const override { return 44100; }
    int channels() const override { return 2; }
    std::uint64_t totalSamples() const override { return 44100 * 2 * 10; }

    std::size_t decodeFrames(std::span<int16_t> outInterleaved, std::size_t outFrames) override {
        const std::size_t samples = std::min(outInterleaved.size(), outFrames * 2);
        std::fill_n(outInterleaved.begin(), samples, int16_t{0});
        return samples / 2;
    }

    const std::string& lastError() const override { return error_; }

private:
    bool open_ = false;
    std::string error_;
};

struct SinkState {
    std::vector<std::string> openedDevices;
    std::string rejectedDevice = "<none>"; // matches no real device name
    int liveSinks = 0;
};

class FakeSink final : public IAudioSink {
public:
    explicit FakeSink(SinkState& state) : state_(state) {}
    ~FakeSink() override { close(); }

    bool open(const AudioFormat&, FrameProvider provider, const std::string& deviceName, int) override {
        if (deviceName == state_.rejectedDevice) {
            return false;
        }
        provider_ = std::move(provider);
        device_ = deviceName;
        open_ = true;
        ++state_.liveSinks;
        state_.openedDevices.push_back(deviceName);
        return true;
    }

    void start() override { started_ = true; }
    void stop() override { started_ = false; }

    void close() override {
        if (!open_) return;
        open_ = false;
        --state_.liveSinks;
    }

    bool isOpen() const override { return open_; }
    void setVolume(int percent) override { volume_ = percent; }
    int getVolume() const override { return volume_; }

private:
    SinkState& state_;
    FrameProvider provider_;
    std::string device_;
    bool open_ = false;
    bool started_ = false;
    int volume_ = 100;
};

PlaybackEngine makeEngine(NotificationBus& bus, SinkState& state) {
    return PlaybackEngine(
        &bus,
        [] { return std::make_unique<FakeDecoder>(); },
        [&state](NotificationBus*) { return std::make_unique<FakeSink>(state); });
}

bool hasMessageContaining(NotificationBus& bus, const std::string& needle) {
    for (const auto& n : bus.drain()) {
        if (n.message.find(needle) != std::string::npos) return true;
    }
    return false;
}

} // namespace

TEST_CASE("Unopenable output device is rejected and current device is kept") {
    NotificationBus bus;
    SinkState state;
    state.rejectedDevice = "AirPods";

    PlaybackEngine engine = makeEngine(bus, state);
    REQUIRE(engine.setOutputDevice("Speakers"));
    REQUIRE(engine.load("fake.mp3"));
    engine.play();
    REQUIRE(engine.isPlaying());

    REQUIRE_FALSE(engine.setOutputDevice("AirPods"));

    CHECK(engine.getOutputDevice() == "Speakers");
    CHECK(engine.isPlaying());
    CHECK(state.liveSinks == 1);
    bus.drain();
}

TEST_CASE("Switching to a working device swaps the sink without stopping playback") {
    NotificationBus bus;
    SinkState state;

    PlaybackEngine engine = makeEngine(bus, state);
    REQUIRE(engine.load("fake.mp3"));
    engine.play();

    REQUIRE(engine.setOutputDevice("Headphones"));

    CHECK(engine.getOutputDevice() == "Headphones");
    CHECK(engine.isPlaying());
    CHECK(state.liveSinks == 1);
    CHECK(state.openedDevices.back() == "Headphones");
    bus.drain();
}

TEST_CASE("Device selected while stopped is probed before being accepted") {
    NotificationBus bus;
    SinkState state;
    state.rejectedDevice = "AirPods";

    PlaybackEngine engine = makeEngine(bus, state);

    REQUIRE_FALSE(engine.setOutputDevice("AirPods"));
    CHECK(engine.getOutputDevice().empty());
    CHECK(hasMessageContaining(bus, "unavailable"));

    REQUIRE(engine.setOutputDevice("Speakers"));
    CHECK(engine.getOutputDevice() == "Speakers");
    CHECK(state.liveSinks == 0);
    bus.drain();
}

TEST_CASE("Load falls back to the system default when the stored device vanished") {
    NotificationBus bus;
    SinkState state;

    PlaybackEngine engine = makeEngine(bus, state);
    REQUIRE(engine.setOutputDevice("Speakers"));

    state.rejectedDevice = "Speakers"; // device disappears after selection
    REQUIRE(engine.load("fake.mp3"));

    CHECK(engine.getOutputDevice().empty());
    CHECK(state.liveSinks == 1);
    engine.play();
    CHECK(engine.isPlaying());
    bus.drain();
}
