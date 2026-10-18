#include "App.h"

#include <chrono>
#include <cstdlib>
#include <future>
#include <string>

#include <ftxui/component/event.hpp>

#include "tui/Palette.h"
#include "tui/ui.h"

namespace {

constexpr auto kRemoteTimeout = std::chrono::seconds(2);

} // namespace

App::App()
    : reloadFlag_(std::make_shared<std::atomic<bool>>(false))
    , visualIndex_(std::make_shared<int>(clampVisual(core_.config())))
    , screen_(ftxui::App::Fullscreen())
    , dispatcher_(core_.service(), core_.config(), *this)
    , refreshTimer_(screen_, std::chrono::milliseconds(33))
    , dirWatcher_(core_.config(), [this](const std::string& root) {
        std::string error;
        core_.directService().setRootPath(root, error);
        *reloadFlag_ = true;
        screen_.PostEvent(ftxui::Event::Custom);
    })
    , controlServer_(resolveSocketPath(), [this](std::string_view request) {
        return handleRemote(request);
    }) {
    int savedTheme = core_.config().getTheme();
    if (savedTheme >= 0 && savedTheme <= 3)
        Palette::setGradient(static_cast<Palette::Theme>(savedTheme));

    if (!controlServer_.listening()) {
        core_.bus().push("Remote control off: " + controlServer_.error(), NotifyLevel::Error);
    }

    component_ = buildTui(core_.service(), dispatcher_, screen_, reloadFlag_, visualIndex_);
}

int App::clampVisual(IConfigService& config) {
    int visual = config.getVisual();
    if (visual < 0 || visual > 4) visual = 0;
    return visual;
}

int App::run() {
    screen_.Loop(component_);
    return EXIT_SUCCESS;
}

void App::applyTheme(int index) {
    Palette::setGradient(static_cast<Palette::Theme>(index));
}

void App::applyVisual(int index) {
    *visualIndex_ = index;
}

void App::libraryChanged() {
    *reloadFlag_ = true;
}

void App::requestQuit() {
    screen_.Exit();
}

std::string App::handleRemote(std::string_view request) {
    // The dispatcher and the UI state it touches belong to the UI thread.
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();

    screen_.Post([this, promise, line = std::string(request)] {
        CommandResult result = dispatcher_.execute(line);
        if (!result.readOnly) {
            std::string summary = result.message.substr(0, result.message.find('\n'));
            core_.bus().push("ctl: " + summary, result.ok ? NotifyLevel::Info : NotifyLevel::Error);
        }
        promise->set_value(std::move(result));
    });
    screen_.PostEvent(ftxui::Event::Custom);

    if (future.wait_for(kRemoteTimeout) != std::future_status::ready) {
        return CommandResult::failure("Player did not respond in time").toJson();
    }
    return future.get().toJson();
}
