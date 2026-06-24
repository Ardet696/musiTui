#include "FileManager.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

#include "../service/ILibraryQuery.h"
#include "../service/IPlaybackControl.h"
#include "ftxui/component/event.hpp"

// Wraps a Menu component to add mouse scroll wheel support.
static ftxui::Component WithMouseScroll(ftxui::Component menu, int* selected, const std::vector<std::string>* items) {
  using namespace ftxui;
  return CatchEvent(menu, [=](Event event) {
    if (event.is_mouse()) {
      auto& mouse = event.mouse();
      if (mouse.button == Mouse::WheelUp && *selected > 0) {
        (*selected)--;
        return true;
      }
      if (mouse.button == Mouse::WheelDown && *selected < (int)items->size() - 1) {
        (*selected)++;
        return true;
      }
    }
    return false;
  });
}

ftxui::Component CreateFileManager(ILibraryQuery& query, IPlaybackControl& control,
                                   const std::shared_ptr<std::atomic<bool>>& reload_flag) {
  using namespace ftxui;
  auto albums = std::make_shared<std::vector<std::string>>(
    query.getAlbumNames()
  );
  auto songs_per_album = std::make_shared<std::vector<std::vector<std::string>>>(
    query.getAllSongNames()
  );
  auto album_loaded = std::make_shared<std::vector<bool>>(
    query.getAlbumLoadedStates()
  );
  auto last_version = std::make_shared<std::uint32_t>(query.getDataVersion());

  auto selected_album = std::make_shared<int>(0);
  auto selected_song  = std::make_shared<int>(0);
  auto viewing_songs  = std::make_shared<bool>(false);
  auto tab_index      = std::make_shared<int>(0);

  auto current_songs = std::make_shared<std::vector<std::string>>(
    albums->empty() ? std::vector<std::string>{} : (*songs_per_album)[0]
  );

  auto albumOpt = MenuOption();
  albumOpt.entries_option.transform = [album_loaded](const EntryState& state) {
    const bool loaded = state.index < (int)album_loaded->size()
                          ? (*album_loaded)[state.index] : true;
    auto label = text("\u25B8 " + state.label + (loaded ? "" : "  \u27F3"));
    if (state.focused) {
      return label | bold | inverted;
    }
    return label | bold | dim;
  };
  auto album_menu = Menu(albums.get(), selected_album.get(), albumOpt);

  auto songOpt = MenuOption();
  songOpt.entries_option.transform = [](const EntryState& state) {
    auto label = text("\u266A " + state.label);
    if (state.focused) {
      return label | inverted;
    }
    return label;
  };
  auto song_menu  = Menu(current_songs.get(), selected_song.get(), songOpt);

  auto scrollable_album_menu = WithMouseScroll(album_menu, selected_album.get(), albums.get());
  auto scrollable_song_menu  = WithMouseScroll(song_menu, selected_song.get(), current_songs.get());

  auto album_with_enter = CatchEvent(scrollable_album_menu, [=, &control](Event event) {
    if (event == Event::Return && !albums->empty()) {
      *current_songs = (*songs_per_album)[*selected_album];
      *viewing_songs = true;
      *tab_index = 1;
      *selected_song = 0;
      control.playSong((*albums)[*selected_album], 0);
      return true;
    }
    return false;
  });

  auto song_with_events = CatchEvent(scrollable_song_menu, [=, &control](Event event) {
    if (event == Event::Return && !current_songs->empty()) {
      control.playSong((*albums)[*selected_album], *selected_song);
      return true;
    }
    if (event == Event::Backspace) {
      *viewing_songs = false;
      *tab_index = 0;
      return true;
    }
    return false;
  });

  auto container = Container::Tab({album_with_enter, song_with_events}, tab_index.get());

  return Renderer(container, [=, &query] {
    if (*reload_flag) {
      *albums = query.getAlbumNames();
      *songs_per_album = query.getAllSongNames();
      *album_loaded = query.getAlbumLoadedStates();
      *current_songs = albums->empty() ? std::vector<std::string>{} : (*songs_per_album)[0];
      *selected_album = 0;
      *selected_song = 0;
      *viewing_songs = false;
      *tab_index = 0;
      *last_version = query.getDataVersion();
      *reload_flag = false;
    } else if (std::uint32_t v = query.getDataVersion(); v != *last_version) {
      *albums = query.getAlbumNames();
      *songs_per_album = query.getAllSongNames();
      *album_loaded = query.getAlbumLoadedStates();
      if (*selected_album >= (int)albums->size())
        *selected_album = albums->empty() ? 0 : (int)albums->size() - 1;
      if (*viewing_songs && !albums->empty()) {
        *current_songs = (*songs_per_album)[*selected_album];
        if (*selected_song >= (int)current_songs->size())
          *selected_song = current_songs->empty() ? 0 : (int)current_songs->size() - 1;
      }
      *last_version = v;
    }

    Element content;
    if (albums->empty()) {
      content = vbox({
        text("FileManager") | bold | center,
        separator(),
        text("") | flex,
        text("Waiting for root path,") | center,
        text("type 'RootConfig' in input") | center,
        text("bar to set it up") | center,
        text("") | flex,
      });
    } else if (*viewing_songs) {
      content = vbox({
        text(" < " + (*albums)[*selected_album]) | bold,
        separator(),
        song_with_events->Render() | yframe | yflex,
      });
    } else {
      content = vbox({
        text("Albums") | bold | center,
        separator(),
        album_with_enter->Render() | yframe | yflex,
      });
    }
    return content | border | flex;
  });
}
