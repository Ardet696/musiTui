#include <catch2/catch_all.hpp>

#include <map>
#include <string>
#include <vector>

#include "../../src/commands/CommandDispatcher.h"
#include "../../src/commands/IUiBridge.h"
#include "../../src/events/NotificationBus.h"
#include "../../src/service/IConfigService.h"
#include "../../src/service/ILibraryService.h"

namespace {

class FakeService : public ILibraryService {
public:
    std::map<std::string, std::vector<std::string>> albums{
        {"Abbey Road", {"Come Together", "Something"}},
        {"Album 2", {"One"}},
        {"Loading", {}},
    };
    std::vector<std::string> devices{"Speakers", "Headphones"};
    std::vector<std::string> calls;
    int volume = 50;
    bool playing = false;
    std::string currentAlbum;
    int currentTrack = -1;

    std::vector<std::string> getAlbumNames() const override {
        std::vector<std::string> names;
        for (const auto& [name, _] : albums) names.push_back(name);
        return names;
    }
    std::vector<bool> getAlbumLoadedStates() const override {
        std::vector<bool> states;
        for (const auto& [_, songs] : albums) states.push_back(!songs.empty());
        return states;
    }
    std::uint32_t getDataVersion() const override { return 0; }
    std::vector<std::string> getSongNames(const std::string& album) const override {
        auto it = albums.find(album);
        return it == albums.end() ? std::vector<std::string>{} : it->second;
    }
    std::vector<std::vector<std::string>> getAllSongNames() const override { return {}; }
    bool setRootPath(const std::string& path, std::string& outError) override {
        if (path.starts_with("/bad")) {
            outError = "does not exist";
            return false;
        }
        calls.push_back("root:" + path);
        return true;
    }
    std::string getCurrentAlbum() const override { return currentAlbum; }
    int getCurrentTrackIndex() const override { return currentTrack; }
    std::string getCurrentSongName() const override {
        return currentTrack < 0 ? "" : getSongNames(currentAlbum)[currentTrack];
    }
    std::string getNextSongName() const override { return ""; }

    void playSong(const std::string& album, int songIndex) override {
        calls.push_back("playSong:" + album + ":" + std::to_string(songIndex));
    }
    void prevSong() override { calls.push_back("prev"); }
    void nextSong() override { calls.push_back("next"); }
    void resumePlayback() override { calls.push_back("resume"); }
    void stopPlayback() override { calls.push_back("stop"); }
    bool isPlaying() const override { return playing; }
    void setVolume(int percent) override { volume = percent; }
    int getVolume() const override { return volume; }
    std::vector<std::string> listOutputDevices() const override { return devices; }
    void setOutputDevice(int deviceIndex) override { calls.push_back("output:" + std::to_string(deviceIndex)); }

    std::vector<float> getSpectrumBars() const override { return {}; }
    std::vector<float> getSpectrumMagnitudes() const override { return {}; }
    float getPlaybackProgress() const override { return 0.0f; }
    float getBpmLoadFactor() const override { return 0.0f; }
    NotificationBus& getNotificationBus() override { return bus; }

    NotificationBus bus;
};

class FakeConfig : public IConfigService {
public:
    std::string root;
    int theme = 0;
    int visual = 0;
    std::string getRootPath() const override { return root; }
    void setRootPath(const std::string& path) override { root = path; }
    int getTheme() const override { return theme; }
    void setTheme(int t) override { theme = t; }
    int getVisual() const override { return visual; }
    void setVisual(int v) override { visual = v; }
};

class FakeUi : public IUiBridge {
public:
    int theme = -1;
    int visual = -1;
    bool reloaded = false;
    bool quit = false;
    void applyTheme(int index) override { theme = index; }
    void applyVisual(int index) override { visual = index; }
    void libraryChanged() override { reloaded = true; }
    void requestQuit() override { quit = true; }
};

struct Fixture {
    FakeService service;
    FakeConfig config;
    FakeUi ui;
    CommandDispatcher dispatcher{service, config, ui};
};

} // namespace

TEST_CASE("tokenize honours quotes and escapes", "[commands]") {
    using V = std::vector<std::string>;
    CHECK(CommandDispatcher::tokenize("a  b\tc") == V{"a", "b", "c"});
    CHECK(CommandDispatcher::tokenize("\"Abbey Road\" 2") == V{"Abbey Road", "2"});
    CHECK(CommandDispatcher::tokenize(R"("say \"hi\"" x)") == V{"say \"hi\"", "x"});
    CHECK(CommandDispatcher::tokenize("\"\"") == V{""});
    CHECK(CommandDispatcher::tokenize("   ").empty());
}

TEST_CASE("quoteArg round-trips through tokenize", "[commands]") {
    for (std::string arg : {"plain", "with space", "q\"uote", "back\\slash", ""}) {
        auto tokens = CommandDispatcher::tokenize(CommandDispatcher::quoteArg(arg));
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0] == arg);
    }
}

TEST_CASE("transport commands reach the service", "[commands]") {
    Fixture f;
    CHECK(f.dispatcher.execute("play").ok);
    CHECK(f.dispatcher.execute("stop").ok);
    CHECK(f.dispatcher.execute("pause").ok);
    CHECK(f.dispatcher.execute("next").ok);
    CHECK(f.dispatcher.execute("  prev  ").ok);
    CHECK(f.service.calls == std::vector<std::string>{"resume", "stop", "stop", "next", "prev"});
}

TEST_CASE("play resolves albums and 1-based tracks", "[commands]") {
    Fixture f;

    SECTION("unquoted name") {
        CHECK(f.dispatcher.execute("play Abbey Road").ok);
        CHECK(f.service.calls.back() == "playSong:Abbey Road:0");
    }
    SECTION("unquoted name with track") {
        CHECK(f.dispatcher.execute("play Abbey Road 2").ok);
        CHECK(f.service.calls.back() == "playSong:Abbey Road:1");
    }
    SECTION("name ending in a number is not split") {
        CHECK(f.dispatcher.execute("play Album 2").ok);
        CHECK(f.service.calls.back() == "playSong:Album 2:0");
    }
    SECTION("quoted name with track") {
        CHECK(f.dispatcher.execute("play \"Album 2\" 1").ok);
        CHECK(f.service.calls.back() == "playSong:Album 2:0");
    }
    SECTION("case-insensitive match") {
        CHECK(f.dispatcher.execute("play abbey road").ok);
        CHECK(f.service.calls.back() == "playSong:Abbey Road:0");
    }
    SECTION("errors") {
        CHECK_FALSE(f.dispatcher.execute("play Nope").ok);
        CHECK_FALSE(f.dispatcher.execute("play Abbey Road 3").ok);
        CHECK_FALSE(f.dispatcher.execute("play Abbey Road 0").ok);
        CHECK_FALSE(f.dispatcher.execute("play Loading").ok);
        CHECK(f.service.calls.empty());
    }
}

TEST_CASE("volume sets, adjusts and clamps", "[commands]") {
    Fixture f;
    CHECK(f.dispatcher.execute("volume 30").ok);
    CHECK(f.service.volume == 30);
    CHECK(f.dispatcher.execute("volume +5").ok);
    CHECK(f.service.volume == 35);
    CHECK(f.dispatcher.execute("volume -50").ok);
    CHECK(f.service.volume == 0);
    CHECK(f.dispatcher.execute("volume 250").ok);
    CHECK(f.service.volume == 100);
    CHECK_FALSE(f.dispatcher.execute("volume loud").ok);

    auto query = f.dispatcher.execute("volume");
    CHECK(query.readOnly);
    CHECK(query.data == "{\"volume\":100}");
}

TEST_CASE("output lists and selects devices", "[commands]") {
    Fixture f;
    auto list = f.dispatcher.execute("output list");
    CHECK(list.data == "{\"devices\":[\"Speakers\",\"Headphones\"]}");
    CHECK(f.dispatcher.execute("output 2").ok);
    CHECK(f.service.calls.back() == "output:1");
    CHECK_FALSE(f.dispatcher.execute("output 3").ok);
}

TEST_CASE("theme and visual accept numbers or names", "[commands]") {
    Fixture f;
    CHECK(f.dispatcher.execute("theme forest").ok);
    CHECK(f.ui.theme == 3);
    CHECK(f.config.theme == 3);
    CHECK(f.dispatcher.execute("themes 2").ok);
    CHECK(f.ui.theme == 1);
    CHECK_FALSE(f.dispatcher.execute("theme 5").ok);

    CHECK(f.dispatcher.execute("visual Wave").ok);
    CHECK(f.ui.visual == 4);
    CHECK(f.config.visual == 4);
    CHECK_FALSE(f.dispatcher.execute("visual 0").ok);
}

TEST_CASE("root updates service, config and UI", "[commands]") {
    Fixture f;
    CHECK(f.dispatcher.execute("root /home/me/My Music").ok);
    CHECK(f.service.calls.back() == "root:/home/me/My Music");
    CHECK(f.config.root == "/home/me/My Music");
    CHECK(f.ui.reloaded);

    CHECK(f.dispatcher.execute("RootConfig \"/x y\"").ok);
    CHECK(f.config.root == "/x y");

    auto bad = f.dispatcher.execute("root /bad");
    CHECK_FALSE(bad.ok);
    CHECK(f.config.root == "/x y");
}

TEST_CASE("status reports state as JSON", "[commands]") {
    Fixture f;
    f.service.currentAlbum = "Abbey Road";
    f.service.currentTrack = 1;
    f.service.playing = true;
    f.config.theme = 2;

    auto r = f.dispatcher.execute("status");
    CHECK(r.ok);
    CHECK(r.readOnly);
    CHECK(r.message == "Playing: Something (Abbey Road, track 2), volume 50%");
    CHECK(r.data.find("\"playing\":true") != std::string::npos);
    CHECK(r.data.find("\"track\":2") != std::string::npos);
    CHECK(r.data.find("\"theme\":\"PurpleRain\"") != std::string::npos);
}

TEST_CASE("albums and songs list the library", "[commands]") {
    Fixture f;
    auto albums = f.dispatcher.execute("albums");
    CHECK(albums.data.find("{\"name\":\"Abbey Road\",\"loaded\":true,\"tracks\":2}") != std::string::npos);
    CHECK(albums.data.find("{\"name\":\"Loading\",\"loaded\":false,\"tracks\":0}") != std::string::npos);

    auto songs = f.dispatcher.execute("songs abbey road");
    CHECK(songs.data == "{\"album\":\"Abbey Road\",\"songs\":[\"Come Together\",\"Something\"]}");
    CHECK_FALSE(f.dispatcher.execute("songs").ok);
}

TEST_CASE("quit and unknown commands", "[commands]") {
    Fixture f;
    CHECK(f.dispatcher.execute("quit").ok);
    CHECK(f.ui.quit);

    auto r = f.dispatcher.execute("dance");
    CHECK_FALSE(r.ok);
    CHECK(r.toJson() == "{\"ok\":false,\"message\":\"Unknown command: dance\"}");
    CHECK_FALSE(f.dispatcher.execute("").ok);
}
