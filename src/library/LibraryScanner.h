#ifndef MP3PLAYER_LIBRARYSCANNER_H
#define MP3PLAYER_LIBRARYSCANNER_H

#include <filesystem>
#include <string>
#include <vector>
#include "MusicLibrary.h"

class LibraryScanner {
public:
        struct Options {
                bool recursive_albums = false;
                bool include_hidden = false;
                bool sort_results = true;
        };
        LibraryScanner() = default;
        explicit LibraryScanner(Options& options) : options_(std::move(options)) {}
        /**
         * Throws:
         * - std::filesystem::filesystem_error on I/O errors (implementation may also choose to swallow
         *   errors and skip problematic entries; decide in .cpp).
         */
        MusicLibrary scanRoot(const std::filesystem::path& rootDir) const;

        MusicLibrary scanFromUserInput(const std::string& userInput, std::string* outError = nullptr) const;
private:
        Options options_;


};
#endif