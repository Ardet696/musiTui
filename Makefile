# Thin wrapper over CMake. The build system is still CMake + Ninja; this only
# exists so the common commands are short.
#
#   make          build release
#   make run      build release and launch
#   make debug    build debug
#   make test     build and run the test suites
#   make clean    delete the build directories

BUILD_DIR   ?= cmake-build-release
DEBUG_DIR   ?= cmake-build-debug
TEST_DIR    ?= cmake-build-test
JOBS        ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu)
GENERATOR   ?= Ninja

.PHONY: all build run debug test clean rebuild

all: build

$(BUILD_DIR)/CMakeCache.txt:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -G "$(GENERATOR)"

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) -j$(JOBS)

run: build
	./$(BUILD_DIR)/MP3Player

$(DEBUG_DIR)/CMakeCache.txt:
	cmake -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug -G "$(GENERATOR)"

debug: $(DEBUG_DIR)/CMakeCache.txt
	cmake --build $(DEBUG_DIR) -j$(JOBS)

$(TEST_DIR)/CMakeCache.txt:
	cmake -B $(TEST_DIR) -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -G "$(GENERATOR)"

test: $(TEST_DIR)/CMakeCache.txt
	cmake --build $(TEST_DIR) -j$(JOBS)
	ctest --test-dir $(TEST_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR) $(DEBUG_DIR) $(TEST_DIR)

rebuild: clean build
