#pragma once

// Presentation-side effects of a command. The TUI applies them to the screen,
// headless mode ignores them (the choice is still persisted through config).
class IUiBridge {
public:
    virtual ~IUiBridge() = default;
    virtual void applyTheme(int index) = 0;
    virtual void applyVisual(int index) = 0;
    virtual void libraryChanged() = 0;
    virtual void requestQuit() = 0;
};
