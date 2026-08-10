#!/usr/bin/env bash
#
# Build FTXUI static libraries for the current platform.
# Pre-built Linux x86_64 libs are shipped in the repo.
# macOS arm64 users (or anyone rebuilding) should run this once:
#
#   ./scripts/build-ftxui.sh
#
set -euo pipefail

FTXUI_VERSION="d120f349b7e7b50584ea79a75a2523a5a66a087f"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FTXUI_DIR="$REPO_ROOT/third_party/ftxui"
BUILD_DIR="$(mktemp -d)"

trap 'rm -rf "$BUILD_DIR"' EXIT

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "$OS-$ARCH" in
    linux-x86_64)  PLATFORM="linux-x86_64"  ;;
    darwin-arm64)  PLATFORM="darwin-arm64"   ;;
    darwin-x86_64) PLATFORM="darwin-x86_64"  ;;
    *)             PLATFORM="${OS}-${ARCH}"   ;;
esac

LIB_DIR="$FTXUI_DIR/lib/$PLATFORM"

echo "Building FTXUI $FTXUI_VERSION for $PLATFORM ..."

git clone --depth 1 https://github.com/ArthurSonzogni/FTXUI.git "$BUILD_DIR/src"
git -C "$BUILD_DIR/src" fetch --depth 1 origin "$FTXUI_VERSION"
git -C "$BUILD_DIR/src" checkout "$FTXUI_VERSION"

CMAKE_ARGS=(
    -B "$BUILD_DIR/build"
    -S "$BUILD_DIR/src"
    -DCMAKE_BUILD_TYPE=Release
    -DFTXUI_ENABLE_INSTALL=OFF
    -DFTXUI_BUILD_EXAMPLES=OFF
    -DFTXUI_BUILD_DOCS=OFF
    -DFTXUI_BUILD_TESTS=OFF
)

# macOS: Apple Clang 21+ supports std::jthread natively with -std=c++20.
# Fallback to GCC via Homebrew only if explicitly set in CXX before running this script.
if [[ "$OS" == "darwin" ]]; then
    echo "Using default compiler (Apple Clang)."
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR/build" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

mkdir -p "$LIB_DIR"
cp "$BUILD_DIR/build/libftxui-screen.a"    "$LIB_DIR/"
cp "$BUILD_DIR/build/libftxui-dom.a"       "$LIB_DIR/"
cp "$BUILD_DIR/build/libftxui-component.a" "$LIB_DIR/"

# Update headers
rm -rf "$FTXUI_DIR/include/ftxui"
cp -r "$BUILD_DIR/src/include/ftxui" "$FTXUI_DIR/include/"

echo ""
echo "Done. Libraries installed to: $LIB_DIR"
echo "Headers installed to: $FTXUI_DIR/include/ftxui"
ls -lh "$LIB_DIR"
