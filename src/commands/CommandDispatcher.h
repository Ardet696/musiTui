#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

class ILibraryService;
class IConfigService;
class IUiBridge;

struct CommandResult {
    bool ok = true;
    bool readOnly = false;  // queries are not echoed into the TUI chat log
    std::string message;
    std::string data;       // serialized JSON object, empty when there is none

    static CommandResult success(std::string message, std::string data = {});
    static CommandResult query(std::string message, std::string data);
    static CommandResult failure(std::string message);

    // One-line wire form: {"ok":bool,"message":"...","data":{...}}
    std::string toJson() const;
};

/**
 * CommandDispatcher - single entry point for every player command.
 *
 * Shared by the TUI command bar and the control socket so both speak the same
 * language. Not thread-safe: callers serialize access (the TUI routes socket
 * requests onto its own thread).
 */
class CommandDispatcher {
public:
    static constexpr std::array<std::string_view, 4> kThemes = {"Fire", "BW", "PurpleRain", "Forest"};
    static constexpr std::array<std::string_view, 5> kVisuals = {"Spectrum", "Oscilloscope", "Mirrored", "Rolling", "Wave"};

    CommandDispatcher(ILibraryService& service, IConfigService& config, IUiBridge& ui);

    CommandResult execute(std::string_view line);

    // Splits arguments on whitespace, honouring "double quotes" and \ escapes.
    static std::vector<std::string> tokenize(std::string_view text);
    // Inverse of tokenize for a single argument.
    static std::string quoteArg(std::string_view arg);

private:
    CommandResult help() const;
    CommandResult status() const;
    CommandResult albums() const;
    CommandResult songs(std::string_view args) const;
    CommandResult play(std::string_view args);
    CommandResult volume(std::string_view args);
    CommandResult output(std::string_view args);
    CommandResult theme(std::string_view args);
    CommandResult visual(std::string_view args);
    CommandResult root(std::string_view args);

    std::string resolveAlbum(std::string_view name) const;

    ILibraryService& service_;
    IConfigService& config_;
    IUiBridge& ui_;
};
