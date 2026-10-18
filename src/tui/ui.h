#ifndef MP3PLAYER_UI_H
#define MP3PLAYER_UI_H

#include <ftxui/component/component.hpp>
#include <ftxui/component/app.hpp>
#include <atomic>
#include <memory>

class ILibraryService;
class CommandDispatcher;

ftxui::Component buildTui(
    ILibraryService& service,
    CommandDispatcher& dispatcher,
    ftxui::App& screen,
    std::shared_ptr<std::atomic<bool>> reloadFlag,
    std::shared_ptr<int> visualIndex);

#endif
