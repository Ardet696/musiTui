#include <catch2/catch_all.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#include "../../src/commands/CommandQueue.h"
#include "../../src/events/NotificationBus.h"
#include "../../src/library/Album.h"
#include "../../src/library/LibraryScanner.h"
#include "../../src/library/MusicLibrary.h"
#include "../../src/player/PlaybackController.h"
#include "../../src/service/LibraryService.h"

namespace {

struct TempLibrary {
    std::filesystem::path root;

    explicit TempLibrary(const std::vector<std::pair<std::string, int>>& albums) {
        std::random_device rd;
        root = std::filesystem::temp_directory_path() /
               ("mp3player_lib_" + std::to_string(rd()) + std::to_string(rd()));
        for (const auto& [name, songCount] : albums) {
            std::filesystem::create_directories(root / name);
            for (int i = 0; i < songCount; ++i) {
                std::ofstream(root / name / ("track" + std::to_string(i) + ".mp3"))
                    << "not-a-real-mp3";
            }
        }
    }

    ~TempLibrary() { std::filesystem::remove_all(root); }
};

} // namespace

TEST_CASE("Scanner produces unloaded skeleton albums", "[library][async]") {
    TempLibrary lib({{"Alpha", 2}, {"Beta", 5}});
    LibraryScanner scanner;

    MusicLibrary library = scanner.scanRoot(lib.root);

    REQUIRE(library.getNumAlbums() == 2);
    for (const auto& album : library.getAlbums()) {
        CHECK_FALSE(album.isLoaded());
        CHECK(album.getNumSongs() == 0);  // songs not decoded yet
    }
}

TEST_CASE("Album::load transitions skeleton to loaded", "[library][async]") {
    TempLibrary lib({{"Alpha", 3}});
    Album album(lib.root / "Alpha", Album::SkeletonTag{});

    REQUIRE_FALSE(album.isLoaded());
    REQUIRE(album.getNumSongs() == 0);

    album.load();

    CHECK(album.isLoaded());
    CHECK(album.getNumSongs() == 3);
}

TEST_CASE("LibraryService loads albums in the background", "[library][async]") {
    TempLibrary lib({{"Alpha", 2}, {"Beta", 3}, {"Gamma", 1}});

    NotificationBus bus;
    CommandQueue queue;
    PlaybackController controller(bus, queue);
    MusicLibrary library;
    LibraryService service(library, controller, bus);

    std::string error;
    REQUIRE(service.setRootPath(lib.root.string(), error));
    REQUIRE(error.empty());

    // Album names appear immediately (skeleton), before songs finish loading.
    CHECK(service.getAlbumNames().size() == 3);

    const std::uint32_t startVersion = service.getDataVersion();

    // Poll until every album reports loaded (bounded wait).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool allLoaded = false;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto states = service.getAlbumLoadedStates();
        allLoaded = !states.empty() &&
                    std::all_of(states.begin(), states.end(), [](bool b) { return b; });
        if (allLoaded) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(allLoaded);
    CHECK(service.getDataVersion() > startVersion);
    CHECK(service.getSongNames("Beta").size() == 3);
}
