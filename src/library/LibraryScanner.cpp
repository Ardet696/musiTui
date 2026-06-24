#include "LibraryScanner.h"
#include "../util/PathValidator.h"

MusicLibrary LibraryScanner::scanRoot(const std::filesystem::path& rootDir) const {
    MusicLibrary library;

    if (!std::filesystem::exists(rootDir) || !std::filesystem::is_directory(rootDir)) {
        return library;
    }

    for (const auto& entry : std::filesystem::directory_iterator(rootDir)) {
        if (!entry.is_directory()) {
            continue;
        }

        if (!options_.include_hidden && entry.path().filename().string()[0] == '.') {
            continue;
        }

        const std::filesystem::path& dirPath = entry.path();
        const std::string dirName = dirPath.filename().string();

        bool hasMp3Files = false;
        bool hasSubdirectories = false;

        try {
            for (const auto& innerEntry : std::filesystem::directory_iterator(dirPath)) {
                if (innerEntry.is_directory()) {
                    hasSubdirectories = true;
                }
                if (innerEntry.is_regular_file() && innerEntry.path().extension() == ".mp3") {
                    hasMp3Files = true;
                }
            }
        } catch (const std::exception&) {
            continue;
        }

        if (hasSubdirectories) {
            continue;
        }
        if (!hasMp3Files) {
            continue;
        }

        if (dirName.find("Playlist") != std::string::npos || dirName.find("playlist") != std::string::npos ||
         dirName.find("Mix") != std::string::npos) {

            try {
                Playlist playlist(dirPath);
                library.addPlaylist(std::move(playlist));
            } catch (const std::exception&) {
            }
        } else {
            try {
                Album album(dirPath, Album::SkeletonTag{});
                library.addAlbum(std::move(album));
            } catch (const std::exception&) {
            }
        }
    }

    return library;
}

MusicLibrary LibraryScanner::scanFromUserInput(const std::string& userInput,
                                                std::string* outError) const {
    PathValidator::Options validatorOptions;
    validatorOptions.requireExists = true;
    validatorOptions.requireDirectory = true;
    validatorOptions.allowRelativePaths = false;
    validatorOptions.strictMode = true;

    PathValidator validator(validatorOptions);
    PathValidator::ValidationResult result = validator.validate(userInput);

    if (!result.valid) {
        if (outError) {
            *outError = result.errorMessage;
        }
        return {};
    }

    return scanRoot(std::filesystem::path(result.sanitizedPath));
}
