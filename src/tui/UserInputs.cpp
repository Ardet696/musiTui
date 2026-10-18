#include "UserInputs.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <chrono>
#include <deque>

#include "../service/ILibraryService.h"
#include "../commands/CommandDispatcher.h"
#include "../events/NotificationBus.h"
#include "ftxui/component/event.hpp"
#include "Palette.h"

namespace {
enum class InputMode { Command, RootConfig, ThemeConfig, OutputDevice, VisualsConfig, Volume };

constexpr std::string_view CMD_HELP  = "help";
constexpr std::string_view CMD_F_HELP = "fileHelp";
constexpr std::string_view CMD_THEMES = "themes";
constexpr std::string_view CMD_OUTPUT = "output";
constexpr std::string_view CMD_VISUALS = "visuals";
constexpr std::string_view CMD_VOLUME = "volume";
constexpr std::string_view CMD_ROOT = "RootConfig";

enum class MsgType { System, User, Error };

struct ChatMessage {
  std::string text;
  MsgType type;
  std::chrono::steady_clock::time_point timestamp;
};

std::string truncateDeviceName(const std::string& name, std::size_t maxChars = 20) {
    if (name.empty()) return name;
    if (name.size() <= maxChars) return name;

    std::string work = name;

    const std::string outputSuffix = " Output";
    if (work.size() > outputSuffix.size() &&
        work.compare(work.size() - outputSuffix.size(), outputSuffix.size(), outputSuffix) == 0) {
        work.erase(work.size() - outputSuffix.size());
    }

    auto hdmiPos = work.find("HDMI / DisplayPort");
    if (hdmiPos != std::string::npos) {
        std::string tail = work.substr(hdmiPos + 18);
        work = "HDMI/DP" + tail;
        if (work.size() <= maxChars) return work;
    }

    std::istringstream iss(work);
    std::vector<std::string> words;
    std::string w;
    while (iss >> w) words.push_back(w);

    if (words.size() <= 2) return work.substr(0, maxChars);

    std::string result = words[words.size() - 2] + " " + words[words.size() - 1];
    if (result.size() <= maxChars) return result;
    return result.substr(0, maxChars);
}

std::string buildDeviceListPrompt(const std::vector<std::string>& devices) {
    if (devices.empty()) return "No output devices found.";

    std::string result = "Select output: ";
    for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
        result += "[" + std::to_string(i + 1) + "]." + truncateDeviceName(devices[i]);
        if (i + 1 < static_cast<int>(devices.size())) result += " ";
    }
    return result;
}

std::string modeLabel(InputMode m) {
  switch (m) {
    case InputMode::RootConfig:   return "RootConfig";
    case InputMode::ThemeConfig:  return "Themes";
    case InputMode::OutputDevice: return "Output";
    case InputMode::VisualsConfig: return "Visuals";
    case InputMode::Volume:       return "Volume";
    default: return "";
  }
}

}

ftxui::Component CreateUserInputs(ILibraryService& service, CommandDispatcher& dispatcher) {
  using namespace ftxui;
  auto input_content = std::make_shared<std::string>();
  auto mode = std::make_shared<InputMode>(InputMode::Command);
  auto chatLog = std::make_shared<std::deque<ChatMessage>>();

  chatLog->push_back({"Type 'help' for commands", MsgType::System, std::chrono::steady_clock::now()});

  auto addMsg = [chatLog](const std::string& msg, MsgType type) {
    chatLog->push_back({msg, type, std::chrono::steady_clock::now()});
    while (chatLog->size() > 8) chatLog->pop_front();
  };

  auto input_option = InputOption();
  input_option.placeholder = "enter command...";
  auto input = Input(input_content.get(), "", input_option);

  // Bare words open a guided prompt; everything else (including one-shot
  // forms like "theme 2") goes straight to the dispatcher.
  auto runCommand = [=, &dispatcher](const std::string& line) {
    auto result = dispatcher.execute(line);
    MsgType type = result.ok ? MsgType::System : MsgType::Error;
    std::istringstream lines(result.message);
    for (std::string msgLine; std::getline(lines, msgLine);) addMsg(msgLine, type);
  };

  auto component = CatchEvent(input, [=, &service, &dispatcher](Event event) mutable {
    // Esc cancels any multi-step mode
    if (event == Event::Escape && *mode != InputMode::Command) {
      addMsg("Cancelled " + modeLabel(*mode), MsgType::System);
      *mode = InputMode::Command;
      input_content->clear();
      return true;
    }

    // Volume mode: arrow keys to adjust, Esc to exit (handled above)
    if (*mode == InputMode::Volume) {
      if (event == Event::ArrowRight) {
        dispatcher.execute("volume +5");
        return true;
      }
      if (event == Event::ArrowLeft) {
        dispatcher.execute("volume -5");
        return true;
      }
      // Consume all other input in volume mode (don't let typing happen)
      if (event.is_character() || event == Event::Return) {
        return true;
      }
      return false;
    }

    if (event != Event::Return || input_content->empty()) return false;

    const std::string& inp = *input_content;
    addMsg(inp, MsgType::User);

    switch (*mode) {
      case InputMode::Command:
        if (inp == CMD_ROOT) {
          *mode = InputMode::RootConfig;
          addMsg("Enter new root path (Esc to cancel):", MsgType::System);
        } else if (inp == CMD_THEMES) {
          *mode = InputMode::ThemeConfig;
          addMsg("[1]-Fire [2]-BW [3]-PurpleRain [4]-Forest (Esc to cancel)", MsgType::System);
        } else if (inp == CMD_OUTPUT) {
          auto devices = service.listOutputDevices();
          if (!devices.empty()) {
            *mode = InputMode::OutputDevice;
            addMsg(buildDeviceListPrompt(devices) + " (Esc to cancel)", MsgType::System);
          } else {
            addMsg("No output devices found.", MsgType::Error);
          }
        } else if (inp == CMD_VISUALS) {
          *mode = InputMode::VisualsConfig;
          addMsg("[1]-Spectrum [2]-Oscilloscope [3]-Mirrored [4]-Rolling [5]-Wave (Esc to cancel)", MsgType::System);
        } else if (inp == CMD_VOLUME) {
          *mode = InputMode::Volume;
          addMsg("Volume: Left/Right arrows to adjust, Esc to exit", MsgType::System);
        } else if (inp == CMD_HELP) {
          addMsg("play, stop, next, prev, status, albums, volume, output, visuals, themes, quit/q", MsgType::System);
        } else if (inp == CMD_F_HELP) {
          addMsg("Albums: Up/Down, Enter=select, Backspace=back", MsgType::System);
        } else {
          runCommand(inp);
        }
        break;
      case InputMode::RootConfig:
        runCommand("root " + CommandDispatcher::quoteArg(inp));
        *mode = InputMode::Command;
        break;
      case InputMode::ThemeConfig:
        runCommand("theme " + CommandDispatcher::quoteArg(inp));
        *mode = InputMode::Command;
        break;
      case InputMode::OutputDevice:
        runCommand("output " + CommandDispatcher::quoteArg(inp));
        *mode = InputMode::Command;
        break;
      case InputMode::VisualsConfig:
        runCommand("visual " + CommandDispatcher::quoteArg(inp));
        *mode = InputMode::Command;
        break;
      case InputMode::Volume:
        break;
    }
    input_content->clear();
    return true;
  });

  return Renderer(component, [component, chatLog, mode, &service] {
    auto now = std::chrono::steady_clock::now();

    for (auto& n : service.getNotificationBus().drain()) {
      MsgType type = (n.level == NotifyLevel::Error) ? MsgType::Error : MsgType::System;
      chatLog->push_back({std::move(n.message), type, now});
      while (chatLog->size() > 8) chatLog->pop_front();
    }

    const auto& gradient = Palette::getCurrentGradient();
    Color dotColor = gradient.size() > 4 ? gradient[4] : Color::White;
    Color userDotColor = gradient.size() > 8 ? gradient[8] : Color::GrayLight;
    while (chatLog->size() > 1) {
      auto& front = chatLog->front();
      auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - front.timestamp).count();
      long limit = (front.type == MsgType::User) ? 2000 : 6000;
      if (age > limit) {
        chatLog->pop_front();
      } else {
        break;
      }
    }

    // Next track header
    std::string nextTrack = service.getNextSongName();
    Element header;
    if (!nextTrack.empty()) {
      header = hbox({
        text("  Next: ") | dim,
        text(nextTrack) | color(dotColor),
      });
    } else {
      header = text("  --") | dim;
    }

    // Build chat log elements
    Elements chatElements;
    for (const auto& msg : *chatLog) {
      if (msg.type == MsgType::Error) {
        chatElements.push_back(hbox({
          text(" x ") | color(Color::Red),
          text(msg.text) | color(Color::Red),
        }));
      } else if (msg.type == MsgType::System) {
        chatElements.push_back(hbox({
          text(" > ") | color(dotColor),
          text(msg.text) | dim,
        }));
      } else {
        chatElements.push_back(hbox({
          text(" $ ") | color(userDotColor),
          text(msg.text),
        }));
      }
    }

    while (static_cast<int>(chatElements.size()) < 6) {
      chatElements.push_back(text(""));
    }

    // Volume mode: show gauge bar instead of input
    if (*mode == InputMode::Volume) {
      int vol = service.getVolume();
      float volF = static_cast<float>(vol) / 100.0f;
      Color barColor = gradient.size() > 6 ? gradient[6] : Color::White;

      return vbox({
        separator(),
        header,
        separator(),
        vbox(std::move(chatElements)) | flex,
        separator(),
        hbox({
          text("  " + std::to_string(vol) + "% ") | bold,
          gauge(volF) | color(barColor) | flex,
        }),
        text("  Left/Right to adjust, Esc to exit") | dim,
      }) | border;
    }

    return vbox({
      separator(),
      header,
      separator(),
      vbox(std::move(chatElements)) | flex,
      component->Render() | border,
    }) | border;
  });
}
