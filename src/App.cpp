#include "App.h"

#include <chrono>
#include <cstdlib>
#include <string>

#include <ftxui/component/event.hpp>

#include "tui/Palette.h"
#include "tui/ui.h"

App::App()
    : controller_(bus_, cmdQueue_)
    , service_(library_, controller_, bus_)
    , asyncService_(service_, cmdQueue_)
    , reloadFlag_(std::make_shared<std::atomic<bool>>(false))
    , visualIndex_(std::make_shared<int>(clampVisual(config_)))
    , screen_(ftxui::App::Fullscreen())
    , refreshTimer_(screen_, std::chrono::milliseconds(33))
    , dirWatcher_(config_, [this](const std::string& root) {
        std::string error;
        service_.setRootPath(root, error);
        *reloadFlag_ = true;
        screen_.PostEvent(ftxui::Event::Custom);
    }) {
    applyConfig();
    component_ = buildTui(asyncService_, config_, screen_, reloadFlag_, visualIndex_);
}

void App::applyConfig() {
    std::string savedRoot = config_.getRootPath();
    if (!savedRoot.empty()) {
        std::string error;
        service_.setRootPath(savedRoot, error);
    }

    int savedTheme = config_.getTheme();
    if (savedTheme >= 0 && savedTheme <= 3)
        Palette::setGradient(static_cast<Palette::Theme>(savedTheme));
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
