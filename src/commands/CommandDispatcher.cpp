#include "CommandDispatcher.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>

#include "IUiBridge.h"
#include "../service/IConfigService.h"
#include "../service/ILibraryService.h"
#include "../util/Json.h"

namespace {

constexpr std::string_view kHelp =
    "play                     resume playback\n"
    "play <album> [track]     play an album, optionally from track N (1-based)\n"
    "stop | pause             pause playback\n"
    "next | prev              skip forward / back\n"
    "status                   current song, album, volume\n"
    "albums                   list albums\n"
    "songs <album>            list the tracks of an album\n"
    "volume [N|+N|-N]         show or set volume (0-100)\n"
    "output [list|N]          list or select the output device\n"
    "theme [N|name]           show or set the color theme\n"
    "visual [N|name]          show or set the visualizer\n"
    "root [path]              show or set the music root directory\n"
    "quit                     close the player";

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

std::optional<int> parseInt(std::string_view s) {
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return value;
}

// Unquoted text is taken verbatim so paths and album names with spaces work
// when typed in the TUI; a leading quote switches to tokenized parsing.
std::string singleArg(std::string_view args) {
    args = trim(args);
    if (!args.empty() && args.front() == '"') {
        auto tokens = CommandDispatcher::tokenize(args);
        return tokens.empty() ? std::string{} : tokens.front();
    }
    return std::string(args);
}

template <std::size_t N>
std::optional<int> pickIndex(std::string_view arg, const std::array<std::string_view, N>& names) {
    if (auto n = parseInt(arg); n && *n >= 1 && *n <= static_cast<int>(N)) return *n - 1;
    for (std::size_t i = 0; i < N; ++i) {
        if (iequals(arg, names[i])) return static_cast<int>(i);
    }
    return std::nullopt;
}

template <std::size_t N>
std::string optionList(const std::array<std::string_view, N>& names) {
    std::string out;
    for (std::size_t i = 0; i < N; ++i) {
        if (i) out += ' ';
        out += '[' + std::to_string(i + 1) + "]-" + std::string(names[i]);
    }
    return out;
}

template <std::size_t N>
std::string jsonNameArray(const std::array<std::string_view, N>& names) {
    std::string out = "[";
    for (std::size_t i = 0; i < N; ++i) {
        if (i) out += ',';
        json::appendString(out, names[i]);
    }
    return out + ']';
}

} // namespace

CommandResult CommandResult::success(std::string message, std::string data) {
    return {true, false, std::move(message), std::move(data)};
}

CommandResult CommandResult::query(std::string message, std::string data) {
    return {true, true, std::move(message), std::move(data)};
}

CommandResult CommandResult::failure(std::string message) {
    return {false, false, std::move(message), {}};
}

std::string CommandResult::toJson() const {
    std::string out = ok ? "{\"ok\":true,\"message\":" : "{\"ok\":false,\"message\":";
    json::appendString(out, message);
    if (!data.empty()) {
        out += ",\"data\":";
        out += data;
    }
    out += '}';
    return out;
}

CommandDispatcher::CommandDispatcher(ILibraryService& service, IConfigService& config, IUiBridge& ui)
    : service_(service), config_(config), ui_(ui)
{}

std::vector<std::string> CommandDispatcher::tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    bool inToken = false;
    bool quoted = false;

    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (c == '\\' && i + 1 < text.size()) {
                current.push_back(text[++i]);
            } else if (c == '"') {
                quoted = false;
            } else {
                current.push_back(c);
            }
        } else if (c == '"') {
            quoted = true;
            inToken = true;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            if (inToken) tokens.push_back(std::move(current));
            current.clear();
            inToken = false;
        } else {
            current.push_back(c);
            inToken = true;
        }
    }
    if (inToken) tokens.push_back(std::move(current));
    return tokens;
}

std::string CommandDispatcher::quoteArg(std::string_view arg) {
    bool needsQuotes = arg.empty() || std::any_of(arg.begin(), arg.end(), [](char c) {
        return c == '"' || c == '\\' || std::isspace(static_cast<unsigned char>(c));
    });
    if (!needsQuotes) return std::string(arg);

    std::string out = "\"";
    for (char c : arg) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out + '"';
}

CommandResult CommandDispatcher::execute(std::string_view line) {
    line = trim(line);
    auto split = line.find_first_of(" \t");
    std::string_view verb = line.substr(0, split);
    std::string_view args = split == std::string_view::npos ? std::string_view{} : trim(line.substr(split));

    if (verb.empty()) return CommandResult::failure("Empty command");
    if (verb == "help") return help();
    if (verb == "status") return status();
    if (verb == "albums") return albums();
    if (verb == "songs") return songs(args);
    if (verb == "play") return play(args);
    if (verb == "stop" || verb == "pause") {
        service_.stopPlayback();
        return CommandResult::success("Stopped");
    }
    if (verb == "next") {
        service_.nextSong();
        return CommandResult::success("Next track");
    }
    if (verb == "prev") {
        service_.prevSong();
        return CommandResult::success("Previous track");
    }
    if (verb == "volume") return volume(args);
    if (verb == "output") return output(args);
    if (verb == "theme" || verb == "themes") return theme(args);
    if (verb == "visual" || verb == "visuals") return visual(args);
    if (verb == "root" || verb == "RootConfig") return root(args);
    if (verb == "quit" || verb == "q") {
        ui_.requestQuit();
        return CommandResult::success("Quitting");
    }
    return CommandResult::failure("Unknown command: " + std::string(verb));
}

CommandResult CommandDispatcher::help() const {
    return CommandResult::query(std::string(kHelp), {});
}

CommandResult CommandDispatcher::status() const {
    std::string album = service_.getCurrentAlbum();
    std::string song = service_.getCurrentSongName();
    std::string next = service_.getNextSongName();
    int track = service_.getCurrentTrackIndex() + 1;
    int volume = service_.getVolume();
    bool playing = service_.isPlaying();
    int themeIdx = std::clamp(config_.getTheme(), 0, static_cast<int>(kThemes.size()) - 1);
    int visualIdx = std::clamp(config_.getVisual(), 0, static_cast<int>(kVisuals.size()) - 1);

    std::string message;
    if (song.empty()) {
        message = "Nothing loaded";
    } else {
        message = std::string(playing ? "Playing: " : "Paused: ") + song +
                  " (" + album + ", track " + std::to_string(track) + ")";
    }
    message += ", volume " + std::to_string(volume) + "%";

    std::string data = "{\"playing\":";
    data += playing ? "true" : "false";
    data += ",\"album\":" + json::quote(album);
    data += ",\"track\":" + std::to_string(song.empty() ? 0 : track);
    data += ",\"song\":" + json::quote(song);
    data += ",\"next\":" + json::quote(next);
    data += ",\"volume\":" + std::to_string(volume);
    data += ",\"theme\":" + json::quote(kThemes[themeIdx]);
    data += ",\"visual\":" + json::quote(kVisuals[visualIdx]);
    data += ",\"root\":" + json::quote(config_.getRootPath());
    data += '}';
    return CommandResult::query(std::move(message), std::move(data));
}

CommandResult CommandDispatcher::albums() const {
    auto names = service_.getAlbumNames();
    auto loaded = service_.getAlbumLoadedStates();
    if (names.empty()) {
        return CommandResult::query("No albums. Set the music root with: root <path>", "{\"albums\":[]}");
    }

    std::string message;
    std::string data = "{\"albums\":[";
    for (std::size_t i = 0; i < names.size(); ++i) {
        bool isLoaded = i < loaded.size() && loaded[i];
        std::size_t tracks = isLoaded ? service_.getSongNames(names[i]).size() : 0;

        if (i) {
            message += '\n';
            data += ',';
        }
        message += std::to_string(i + 1) + ". " + names[i] +
                   (isLoaded ? " (" + std::to_string(tracks) + " tracks)" : " (loading)");
        data += "{\"name\":" + json::quote(names[i]) +
                ",\"loaded\":" + (isLoaded ? "true" : "false") +
                ",\"tracks\":" + std::to_string(tracks) + '}';
    }
    data += "]}";
    return CommandResult::query(std::move(message), std::move(data));
}

CommandResult CommandDispatcher::songs(std::string_view args) const {
    std::string requested = singleArg(args);
    if (requested.empty()) return CommandResult::failure("Usage: songs <album>");

    std::string album = resolveAlbum(requested);
    if (album.empty()) return CommandResult::failure("No album named '" + requested + "' (see: albums)");

    auto names = service_.getSongNames(album);
    std::string message;
    std::string data = "{\"album\":" + json::quote(album) + ",\"songs\":[";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) {
            message += '\n';
            data += ',';
        }
        message += std::to_string(i + 1) + ". " + names[i];
        json::appendString(data, names[i]);
    }
    data += "]}";
    if (names.empty()) message = album + " is still loading";
    return CommandResult::query(std::move(message), std::move(data));
}

CommandResult CommandDispatcher::play(std::string_view args) {
    if (args.empty()) {
        service_.resumePlayback();
        return CommandResult::success("Playing...");
    }

    std::string album;
    std::optional<int> track;

    if (args.front() == '"') {
        auto tokens = tokenize(args);
        if (tokens.empty() || tokens.size() > 2) return CommandResult::failure("Usage: play <album> [track]");
        album = resolveAlbum(tokens[0]);
        if (album.empty()) return CommandResult::failure("No album named '" + tokens[0] + "' (see: albums)");
        if (tokens.size() == 2) {
            track = parseInt(tokens[1]);
            if (!track) return CommandResult::failure("Track must be a number");
        }
    } else {
        // Prefer the whole text as the album name so names ending in a number
        // still resolve; fall back to treating the last word as the track.
        album = resolveAlbum(args);
        if (album.empty()) {
            auto lastSpace = args.find_last_of(" \t");
            if (lastSpace != std::string_view::npos) {
                track = parseInt(args.substr(lastSpace + 1));
                if (track) album = resolveAlbum(trim(args.substr(0, lastSpace)));
            }
        }
        if (album.empty()) return CommandResult::failure("No album named '" + std::string(args) + "' (see: albums)");
    }

    auto songNames = service_.getSongNames(album);
    if (songNames.empty()) return CommandResult::failure(album + " is still loading, retry shortly");

    int number = track.value_or(1);
    if (number < 1 || number > static_cast<int>(songNames.size())) {
        return CommandResult::failure("Track must be 1.." + std::to_string(songNames.size()));
    }

    service_.playSong(album, number - 1);
    return CommandResult::success("Playing " + songNames[number - 1] + " (" + album + ")");
}

CommandResult CommandDispatcher::volume(std::string_view args) {
    int current = service_.getVolume();
    if (args.empty()) {
        return CommandResult::query("Volume " + std::to_string(current) + "%",
                                    "{\"volume\":" + std::to_string(current) + '}');
    }

    bool relative = args.front() == '+' || args.front() == '-';
    auto value = parseInt(args.front() == '+' ? args.substr(1) : args);
    if (!value) return CommandResult::failure("Usage: volume [N|+N|-N]");

    int target = std::clamp(relative ? current + *value : *value, 0, 100);
    service_.setVolume(target);
    return CommandResult::success("Volume " + std::to_string(target) + "%",
                                  "{\"volume\":" + std::to_string(target) + '}');
}

CommandResult CommandDispatcher::output(std::string_view args) {
    auto devices = service_.listOutputDevices();

    if (args.empty() || args == "list") {
        if (devices.empty()) return CommandResult::query("No output devices found", "{\"devices\":[]}");
        std::string message;
        std::string data = "{\"devices\":[";
        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (i) {
                message += '\n';
                data += ',';
            }
            message += std::to_string(i + 1) + ". " + devices[i];
            json::appendString(data, devices[i]);
        }
        data += "]}";
        return CommandResult::query(std::move(message), std::move(data));
    }

    auto n = parseInt(args);
    if (!n || *n < 1 || *n > static_cast<int>(devices.size())) {
        return CommandResult::failure("Invalid device. Enter [1.." + std::to_string(devices.size()) + "]");
    }
    // The switch is asynchronous; success or failure arrives on the notification bus.
    service_.setOutputDevice(*n - 1);
    return CommandResult::success("Switching output to " + devices[*n - 1]);
}

CommandResult CommandDispatcher::theme(std::string_view args) {
    if (args.empty()) {
        int idx = std::clamp(config_.getTheme(), 0, static_cast<int>(kThemes.size()) - 1);
        return CommandResult::query("Theme " + std::string(kThemes[idx]) + ". Options: " + optionList(kThemes),
                                    "{\"theme\":" + json::quote(kThemes[idx]) +
                                    ",\"options\":" + jsonNameArray(kThemes) + '}');
    }

    auto idx = pickIndex(args, kThemes);
    if (!idx) return CommandResult::failure("Invalid theme. Options: " + optionList(kThemes));

    ui_.applyTheme(*idx);
    config_.setTheme(*idx);
    return CommandResult::success("Theme: " + std::string(kThemes[*idx]));
}

CommandResult CommandDispatcher::visual(std::string_view args) {
    if (args.empty()) {
        int idx = std::clamp(config_.getVisual(), 0, static_cast<int>(kVisuals.size()) - 1);
        return CommandResult::query("Visual " + std::string(kVisuals[idx]) + ". Options: " + optionList(kVisuals),
                                    "{\"visual\":" + json::quote(kVisuals[idx]) +
                                    ",\"options\":" + jsonNameArray(kVisuals) + '}');
    }

    auto idx = pickIndex(args, kVisuals);
    if (!idx) return CommandResult::failure("Invalid visual. Options: " + optionList(kVisuals));

    ui_.applyVisual(*idx);
    config_.setVisual(*idx);
    return CommandResult::success("Visual: " + std::string(kVisuals[*idx]));
}

CommandResult CommandDispatcher::root(std::string_view args) {
    std::string path = singleArg(args);
    if (path.empty()) {
        std::string current = config_.getRootPath();
        return CommandResult::query(current.empty() ? "Root not set" : "Root: " + current,
                                    "{\"root\":" + json::quote(current) + '}');
    }

    std::string error;
    if (!service_.setRootPath(path, error)) return CommandResult::failure("Invalid path: " + error);

    config_.setRootPath(path);
    ui_.libraryChanged();
    return CommandResult::success("Root: " + path);
}

std::string CommandDispatcher::resolveAlbum(std::string_view name) const {
    auto names = service_.getAlbumNames();
    for (const auto& n : names) {
        if (n == name) return n;
    }
    std::string match;
    for (const auto& n : names) {
        if (iequals(n, name)) {
            if (!match.empty()) return {};  // ambiguous
            match = n;
        }
    }
    return match;
}
