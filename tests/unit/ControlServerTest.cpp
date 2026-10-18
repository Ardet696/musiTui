#include <catch2/catch_all.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../src/ipc/ControlClient.h"
#include "../../src/ipc/ControlServer.h"
#include "../../src/ipc/SocketIo.h"
#include "../../src/util/Json.h"

namespace {

struct TempSocketDir {
    std::filesystem::path dir;
    TempSocketDir() {
        std::random_device rd;
        // Short name: sun_path is only ~104 bytes on macOS.
        dir = std::filesystem::path("/tmp") / ("mt" + std::to_string(rd() % 1000000));
        std::filesystem::create_directories(dir);
        ::chmod(dir.c_str(), 0700);
    }
    ~TempSocketDir() { std::filesystem::remove_all(dir); }
    SocketLocation location() const { return {dir / "s.sock", false}; }
};

ControlServer::Handler echo(std::atomic<int>* calls = nullptr) {
    return [calls](std::string_view request) {
        if (calls) ++*calls;
        return "{\"ok\":true,\"message\":" + json::quote(request) + "}";
    };
}

} // namespace

TEST_CASE("ControlServer round-trips a request", "[ipc]") {
    TempSocketDir tmp;
    ControlServer server(tmp.location(), echo());
    REQUIRE(server.listening());

    struct stat st {};
    REQUIRE(::stat(tmp.location().path.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    std::string error;
    auto response = sendControlRequest(tmp.location(), "play \"Abbey Road\" 2", error);
    REQUIRE(response);
    CHECK(json::findString(*response, "message") == "play \"Abbey Road\" 2");
}

TEST_CASE("ControlServer serves sequential clients", "[ipc]") {
    TempSocketDir tmp;
    std::atomic<int> calls{0};
    ControlServer server(tmp.location(), echo(&calls));
    REQUIRE(server.listening());

    std::string error;
    for (int i = 0; i < 50; ++i) {
        REQUIRE(sendControlRequest(tmp.location(), "status", error));
    }
    CHECK(calls == 50);
}

TEST_CASE("ControlServer refuses a second live instance", "[ipc]") {
    TempSocketDir tmp;
    ControlServer first(tmp.location(), echo());
    REQUIRE(first.listening());

    ControlServer second(tmp.location(), echo());
    CHECK_FALSE(second.listening());
    CHECK(second.error().find("another musiTui instance") != std::string::npos);
}

TEST_CASE("ControlServer replaces a stale socket", "[ipc]") {
    TempSocketDir tmp;
    auto loc = tmp.location();

    // Bind a socket and close it without unlinking, as a crash would.
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    std::string error;
    REQUIRE(sockio::makeAddress(loc.path, addr, error));
    REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    ::close(fd);
    REQUIRE(std::filesystem::exists(loc.path));

    ControlServer server(loc, echo());
    CHECK(server.listening());
    CHECK(sendControlRequest(loc, "status", error));
}

TEST_CASE("ControlServer leaves a non-socket file alone", "[ipc]") {
    TempSocketDir tmp;
    auto loc = tmp.location();
    { std::ofstream(loc.path) << "keep me"; }

    ControlServer server(loc, echo());
    CHECK_FALSE(server.listening());
    CHECK(std::filesystem::is_regular_file(loc.path));
}

TEST_CASE("ControlServer rejects oversized requests", "[ipc]") {
    TempSocketDir tmp;
    std::atomic<int> calls{0};
    ControlServer server(tmp.location(), echo(&calls));
    REQUIRE(server.listening());

    std::string error;
    auto response = sendControlRequest(tmp.location(), std::string(sockio::kMaxLine + 100, 'x'), error);
    REQUIRE(response);
    CHECK(response->starts_with("{\"ok\":false"));
    CHECK(calls == 0);
}

TEST_CASE("ControlServer stops, joins and removes its socket", "[ipc][thread][raii]") {
    TempSocketDir tmp;
    auto loc = tmp.location();
    {
        ControlServer server(loc, echo());
        REQUIRE(server.listening());
    }
    CHECK_FALSE(std::filesystem::exists(loc.path));

    std::string error;
    CHECK_FALSE(sendControlRequest(loc, "status", error));
}

TEST_CASE("ControlServer creates and verifies a private directory", "[ipc]") {
    TempSocketDir tmp;
    SocketLocation loc{tmp.dir / "sub" / "s.sock", true};
    {
        ControlServer server(loc, echo());
        REQUIRE(server.listening());
        struct stat st {};
        REQUIRE(::stat((tmp.dir / "sub").c_str(), &st) == 0);
        CHECK((st.st_mode & 0777) == 0700);
    }

    ::chmod((tmp.dir / "sub").c_str(), 0755);
    ControlServer insecure(loc, echo());
    CHECK_FALSE(insecure.listening());
}
