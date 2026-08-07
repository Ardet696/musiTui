#include "SdlAudioSubsystem.h"

#include <SDL.h>
#include <mutex>

namespace {

// Retried on every handle construction so a first failure (audio server not up
// yet) does not permanently poison the process. Handles are built from the UI
// and command threads, hence the lock.
bool ensureAudioSubsystem() {
    static std::mutex mutex;
    static bool initialized = false;

    std::lock_guard<std::mutex> lock(mutex);
    if (!initialized) {
        initialized = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    }
    return initialized;
}

} // namespace

SdlAudioSubsystem::SdlAudioSubsystem() : ok_(ensureAudioSubsystem()) {}
