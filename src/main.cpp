#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "App.h"
#include "Daemon.h"
#include "ipc/ControlClient.h"

#ifndef MUSITUI_VERSION
#define MUSITUI_VERSION "dev"
#endif

namespace {

constexpr std::string_view kUsage =
    "usage: musiTui                      start the player (TUI)\n"
    "       musiTui --daemon             start the player headless, controlled via ctl\n"
    "       musiTui ctl [--json] <cmd>   send a command to the running player\n"
    "       musiTui ctl help             list player commands\n"
    "       musiTui --version | --help\n";

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string_view> args(argv + 1, argv + argc);

    if (args.empty()) {
        App app;
        return app.run();
    }

    std::string_view first = args.front();
    if (first == "ctl") return runControlClient(std::span(args).subspan(1));
    if (first == "--daemon") {
        Daemon daemon;
        return daemon.run();
    }
    if (first == "--version" || first == "-v") {
        std::cout << "musiTui " << MUSITUI_VERSION << '\n';
        return EXIT_SUCCESS;
    }
    if (first == "--help" || first == "-h") {
        std::cout << kUsage;
        return EXIT_SUCCESS;
    }

    std::cerr << "musiTui: unknown argument '" << first << "'\n" << kUsage;
    return EXIT_FAILURE;
}
