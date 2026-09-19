cmake_minimum_required(VERSION 3.20)
project(MP3Player VERSION 1.2.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
endif()

set(FTXUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ftxui)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(FTXUI_PLATFORM "linux-x86_64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(FTXUI_PLATFORM "darwin-arm64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(FTXUI_PLATFORM "darwin-x86_64")
else()
    set(FTXUI_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(FTXUI_LIB_DIR ${FTXUI_DIR}/lib/${FTXUI_PLATFORM})

if(NOT EXISTS ${FTXUI_LIB_DIR}/libftxui-screen.a)
    message(FATAL_ERROR
        "Pre-built FTXUI libraries not found for ${FTXUI_PLATFORM}.\n"
        "Run: ./scripts/build-ftxui.sh\n"
        "Then re-run cmake.")
endif()

set(FTXUI_PREVIOUS "")
foreach(lib screen dom component)
    add_library(ftxui-${lib} STATIC IMPORTED)
    set_target_properties(ftxui-${lib} PROPERTIES
        IMPORTED_LOCATION ${FTXUI_LIB_DIR}/libftxui-${lib}.a
        INTERFACE_INCLUDE_DIRECTORIES ${FTXUI_DIR}/include
        INTERFACE_LINK_LIBRARIES "${FTXUI_PREVIOUS}"
    )
    set(FTXUI_PREVIOUS ftxui-${lib})
endforeach()

option(MP3PLAYER_VENDOR_SDL2 "Download and build SDL2 from source instead of using a system install" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MP3PLAYER_ALLOW_VENDOR_SDL2 ${MP3PLAYER_VENDOR_SDL2})
else()
    set(MP3PLAYER_ALLOW_VENDOR_SDL2 ON)
endif()

if(NOT MP3PLAYER_VENDOR_SDL2)
    find_package(SDL2 QUIET CONFIG)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
        endif()
    endif()
endif()

if(NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2 AND NOT TARGET PkgConfig::SDL2)
    if(MP3PLAYER_ALLOW_VENDOR_SDL2)
        message(STATUS "SDL2 not found; fetching and building it from source (release-2.32.10).")
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-2.32.10
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL2)
    else()
        message(FATAL_ERROR
            "SDL2 not found. On Linux, install it from your distro so it links "
            "against the system audio frameworks:\n"
            "  Arch:          sudo pacman -S sdl2\n"
            "  Ubuntu/Debian: sudo apt install libsdl2-dev\n"
            "  Fedora:        sudo dnf install SDL2-devel\n"
            "Or pass -DMP3PLAYER_VENDOR_SDL2=ON to build SDL2 from source anyway.")
    endif()
endif()

if(TARGET SDL2::SDL2)
    set(MP3PLAYER_SDL2_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    set(MP3PLAYER_SDL2_TARGET SDL2::SDL2-static)
else()
    set(MP3PLAYER_SDL2_TARGET PkgConfig::SDL2)
endif()

function(mp3player_configure target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        # Security hardening (non-debug builds parse untrusted audio input)
        $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<NOT:$<CONFIG:Debug>>>:-fstack-protector-strong;-U_FORTIFY_SOURCE;-D_FORTIFY_SOURCE=2>
    )
endfunction()

add_library(mp3player_core STATIC
    src/library/AlbumInfo.cpp
    src/library/LibraryScanner.cpp
    src/library/Playlist.cpp
    src/library/Song.cpp
    src/library/Album.cpp
    src/library/MusicLibrary.cpp

    src/player/SongQueue.cpp
    src/player/PlaybackEngine.cpp
    src/player/DecodeThread.cpp
    src/player/AutoAdvanceManager.cpp
    src/player/PlaybackController.cpp

    src/audio/SdlAudioSink.cpp
    src/audio/SdlAudioSubsystem.cpp

    src/decode/Mp3Decoder.cpp
    src/decode/FlacDecoder.cpp
    src/decode/WavDecoder.cpp
    src/decode/AudioDecoderFactory.cpp
    src/decode/SpectrumAnalyzer.cpp
    src/decode/BpmDetector.cpp
    src/decode/Fft.cpp

    src/util/PathValidator.cpp
    src/util/DirectoryWatcher.cpp

    src/events/PlaybackEventPublisher.cpp

    src/service/LibraryService.cpp
    src/service/ConfigService.cpp
    src/service/AsyncLibraryService.cpp
    src/commands/CommandQueue.cpp
)
mp3player_configure(mp3player_core)
target_link_libraries(mp3player_core PUBLIC ${MP3PLAYER_SDL2_TARGET})

add_executable(MP3Player
    src/main.cpp
    src/App.cpp
    src/tui/ui.cpp
    src/tui/FileManager.cpp
    src/tui/ScreenRefreshTimer.cpp
    src/tui/VisualsBase.cpp
    src/tui/Visuals.cpp
    src/tui/VisualsMirrored.cpp
    src/tui/VisualsRolling.cpp
    src/tui/VisualsCircular.cpp
    src/tui/VisualsPanel.cpp
    src/tui/PlayingBar.cpp
    src/tui/UserInputs.cpp
)

mp3player_configure(MP3Player)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(MP3Player PRIVATE -pie -Wl,-z,relro,-z,now)
endif()

target_link_libraries(MP3Player PRIVATE mp3player_core ftxui-component)

install(TARGETS MP3Player RUNTIME DESTINATION bin)

option(BUILD_TESTING "Build unit tests" OFF)

if(BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    function(mp3player_add_test_suite name)
        add_executable(${name} ${ARGN})
        mp3player_configure(${name})
        target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain mp3player_core)
        target_compile_definitions(${name} PRIVATE
            FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    endfunction()

    mp3player_add_test_suite(unit_tests
        tests/unit/RingBufferTest.cpp
        tests/unit/FftTest.cpp
        tests/unit/SpectrumAnalyzerTest.cpp
        tests/unit/BpmDetectorTest.cpp
        tests/unit/PathValidatorTest.cpp
        tests/unit/ConfigServiceTest.cpp
        tests/unit/Mp3DecoderRaiiTest.cpp
        tests/unit/AudioDecoderFactoryTest.cpp
        tests/unit/DecodeThreadTest.cpp
        tests/unit/ThreadLifecycleTest.cpp
        tests/unit/SdlAudioSinkTest.cpp
        tests/unit/AsyncLibraryLoadTest.cpp
        tests/unit/OutputDeviceSwitchTest.cpp
    )

    mp3player_add_test_suite(stress_tests
        tests/stress/RingBufferStressTest.cpp
    )

    mp3player_add_test_suite(integration_tests
        tests/integration/Mp3DecoderTest.cpp
        tests/integration/FlacDecoderTest.cpp
        tests/integration/WavDecoderTest.cpp
    )

    add_test(NAME unit COMMAND unit_tests)
    add_test(NAME stress COMMAND stress_tests)
    add_test(NAME integration COMMAND integration_tests)
endif()

option(BUILD_FUZZERS "Build libFuzzer targets (requires Clang)" OFF)

if(BUILD_FUZZERS)
    add_executable(fuzz_mp3 tests/fuzz/FuzzMp3.cpp)
    target_compile_features(fuzz_mp3 PRIVATE cxx_std_20)
    target_compile_options(fuzz_mp3 PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_mp3 PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

option(BUILD_BENCHMARKS "Build benchmark executables" OFF)

if(BUILD_BENCHMARKS)
    function(add_bench target)
        add_executable(${target} ${ARGN})
        target_compile_features(${target} PRIVATE cxx_std_20)
        set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -O2>
        )
    endfunction()

    add_bench(CommandQueueBench
        tests/bench/CommandQueueBench.cpp
        src/commands/CommandQueue.cpp
    )

    add_bench(RingBufferBench
        tests/bench/RingBufferBench.cpp
    )

    add_bench(SpectrumBench
        tests/bench/SpectrumBench.cpp
        src/decode/SpectrumAnalyzer.cpp
        src/decode/Fft.cpp
    )

    add_bench(BpmDetectorBench
        tests/bench/BpmDetectorBench.cpp
        src/decode/BpmDetector.cpp
    )
endif()
