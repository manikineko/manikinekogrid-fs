#!/bin/bash
set -euo pipefail

# Self-contained build for Manikineko Online: FS-based viewer
# Works on Linux (native) and Windows (Cygwin + Visual Studio).
#
# Usage:
#   ./build.sh [--rebuild] [--variant ReleaseOS|ReleaseFS_open] [--channel NAME]
#
# On Windows (Cygwin), this script:
#   1. Locates Visual Studio via vswhere.exe
#   2. Loads the MSVC environment (cl.exe, INCLUDE, LIB, etc.) via vcvarsall.bat
#   3. Installs viewer dependencies via autobuild install
#   4. Configures with cmake -G Ninja (bypassing configure_firestorm.sh)
#   5. Builds with ninja
#
# On Linux, it uses the existing autobuild configure/build flow.

ROOT="$(cd "$(dirname "$0")" && pwd)"
VIEWER="$ROOT/viewer"
export AUTOBUILD_VARIABLES_FILE="$ROOT/build-variables/variables"

REBUILD=0
VARIANT="ReleaseOS"
CHANNEL="ManikinekoOnline"
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --rebuild              Remove the build directory before building
  --variant NAME         Autobuild variant: ReleaseOS (default) or ReleaseFS_open
  --channel NAME         Viewer channel name (default: ManikinekoOnline)
  --jobs N               Number of parallel build jobs (default: $JOBS)
  -h, --help             Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rebuild) REBUILD=1; shift ;;
        --variant) VARIANT="$2"; shift 2 ;;
        --channel) CHANNEL="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

cd "$VIEWER"

# --- Set up Python venv + autobuild ---

if [ ! -d .venv ]; then
    python3 -m venv .venv
fi
source .venv/bin/activate
pip install --upgrade pip -q
pip install -r requirements.txt -q

# --- Detect platform ---

PLATFORM=""
case "$(uname -s)" in
    Linux*)  PLATFORM="linux" ;;
    CYGWIN*|MINGW*|MSYS*) PLATFORM="windows" ;;
    Darwin*) PLATFORM="macos" ;;
    *) echo "Unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

echo "[build] Platform: $PLATFORM"
echo "[build] Variant: $VARIANT"
echo "[build] Channel: $CHANNEL"

# --- Windows: load MSVC environment and use cmake directly ---

if [[ "$PLATFORM" == "windows" ]]; then
    export AUTOBUILD_VSVER="${AUTOBUILD_VSVER:-170}"
    export AUTOBUILD_ADDRSIZE="${AUTOBUILD_ADDRSIZE:-64}"

    BUILD_DIR="$VIEWER/build-vc${AUTOBUILD_VSVER}-${AUTOBUILD_ADDRSIZE}"

    if [[ $REBUILD -eq 1 ]]; then
        echo "[build] Removing $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    # Locate Visual Studio via vswhere.exe
    VSWHERE="/cygdrive/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    if [[ ! -x "$VSWHERE" ]]; then
        VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    fi

    VS_INSTALL_DIR=""
    if [[ -x "$VSWHERE" ]]; then
        VS_INSTALL_DIR=$("$VSWHERE" -latest -products '*' \
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
            -property installationPath 2>/dev/null | tr -d '\r')
    fi

    if [[ -z "$VS_INSTALL_DIR" ]]; then
        echo "[build] ERROR: Visual Studio not found." >&2
        echo "[build] Install VS Build Tools or run install_windows_deps.ps1" >&2
        exit 1
    fi

    echo "[build] Visual Studio: $VS_INSTALL_DIR"

    # Load MSVC environment from vcvarsall.bat
    VCVARSALL="$VS_INSTALL_DIR/VC/Auxiliary/Build/vcvarsall.bat"
    if [[ ! -f "$VCVARSALL" ]]; then
        echo "[build] ERROR: vcvarsall.bat not found at $VCVARSALL" >&2
        exit 1
    fi

    VCVARSALL_WIN=$(cygpath -w "$VCVARSALL" 2>/dev/null || echo "$VCVARSALL")

    echo "[build] Loading MSVC environment..."
    MSVC_ENV_FILE=$(mktemp)
    cmd /c "\"$VCVARSALL_WIN\" x64 && set" > "$MSVC_ENV_FILE" 2>/dev/null

    while IFS= read -r line; do
        [[ "$line" =~ ^[A-Za-z_][A-Za-z0-9_]*= ]] || continue
        varname="${line%%=*}"
        varvalue="${line#*=}"
        export "$varname=$varvalue"
    done < "$MSVC_ENV_FILE"
    rm -f "$MSVC_ENV_FILE"

    # Prepend VS cmake to PATH (it supports VS generators if needed)
    VS_CMAKE="$VS_INSTALL_DIR/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
    if [[ -d "$VS_CMAKE" ]]; then
        VS_CMAKE_CYG=$(cygpath -u "$VS_CMAKE" 2>/dev/null || echo "$VS_CMAKE")
        export PATH="$VS_CMAKE_CYG:$PATH"
    fi

    if ! command -v cl.exe &>/dev/null; then
        echo "[build] ERROR: cl.exe not found after loading MSVC environment." >&2
        exit 1
    fi
    echo "[build] MSVC compiler: $(which cl.exe)"

    # Install viewer dependencies (NOT --all, which would try to configure the
    # main firestorm package and call configure_firestorm.sh)
    echo "[build] Installing viewer dependencies..."
    autobuild install 2>&1 || echo "[build] WARNING: autobuild install had issues." >&2

    # Version
    if [[ -d "$VIEWER/.git" ]]; then
        BUILD_VER=$(git -C "$VIEWER" rev-list --count HEAD 2>/dev/null || echo "0")
        GIT_HASH=$(git -C "$VIEWER" describe --always --exclude '*' 2>/dev/null || echo "unknown")
    else
        BUILD_VER="0"
        GIT_HASH="unknown"
    fi

    VERSION_FILE="$VIEWER/indra/newview/VIEWER_VERSION_FS.txt"
    if [[ -f "$VERSION_FILE" ]]; then
        VERSION_LINE=$(head -1 "$VERSION_FILE")
        MAJOR=$(echo "$VERSION_LINE" | cut -d. -f1)
        MINOR=$(echo "$VERSION_LINE" | cut -d. -f2)
        PATCH=$(echo "$VERSION_LINE" | cut -d. -f3)
        VIEWER_VERSION="${MAJOR}.${MINOR}.${PATCH}.${BUILD_VER}"
    else
        VIEWER_VERSION="7.2.5.${BUILD_VER}"
    fi
    echo "[build] Version: $VIEWER_VERSION ($GIT_HASH)"

    # Configure with cmake directly (bypassing configure_firestorm.sh)
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    CMAKE_ARGS=(
        -G "Ninja"
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DADDRESS_SIZE:STRING="$AUTOBUILD_ADDRSIZE"
        -DROOT_PROJECT_NAME:STRING=SecondLife
        -DVIEWER_CHANNEL:STRING="Firestorm-$CHANNEL"
        -DGRID:STRING="\"agni\""
        -DUNATTENDED:BOOL=ON
        -DLL_TESTS:BOOL=OFF
        -DPACKAGE:BOOL=ON
        -DOPENSIM:BOOL=ON
        -DINSTALL_PROPRIETARY=FALSE
        -DUSE_KDU=FALSE
        -DUSE_OPENAL:BOOL=ON
        -DRELEASE_CRASH_REPORTING:BOOL=OFF
        -DVIEWER_SYMBOL_FILE:STRING=""
        -DUSE_AVX_OPTIMIZATION:BOOL=OFF
        -DUSE_AVX2_OPTIMIZATION:BOOL=OFF
    )

    # Lua support
    LUA_DIR="${LUA_DIR:-C:/ManikinekoBuild/lua54}"
    if [[ -d "$LUA_DIR/include" && -d "$LUA_DIR/lib" ]]; then
        echo "[build] Lua: $LUA_DIR"
        CMAKE_ARGS+=(
            -DMKO_WITH_LUA:BOOL=ON
            -DLUA_INCLUDE_DIR="$LUA_DIR/include"
            -DLUA_LIBRARY="$LUA_DIR/lib/lua54.lib"
        )
    else
        echo "[build] Lua not found, building without MKO_WITH_LUA"
        CMAKE_ARGS+=(-DMKO_WITH_LUA:BOOL=OFF)
    fi

    echo "[build] Configuring with cmake..."
    cmake "${CMAKE_ARGS[@]}" ../indra

    echo "[build] Building (this will take a long time)..."
    cmake --build . --parallel "$JOBS"

    echo "[build] Build complete: $BUILD_DIR"

# --- Linux/macOS: use autobuild configure/build ---

else
    export AUTOBUILD_ADDRSIZE="${AUTOBUILD_ADDRSIZE:-64}"

    if [[ $REBUILD -eq 1 ]]; then
        BUILD_DIR="$VIEWER/build-linux-x86_64"
        if [[ "$PLATFORM" == "macos" ]]; then
            BUILD_DIR="$VIEWER/build-darwin-x86_64"
        fi
        echo "[build] Removing $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    echo "[build] Installing viewer dependencies..."
    autobuild install 2>&1 || echo "[build] WARNING: autobuild install had issues." >&2

    echo "[build] Configuring ($VARIANT)..."
    autobuild configure -A "$AUTOBUILD_ADDRSIZE" -c "$VARIANT" -- \
        -DVIEWER_CHANNEL:STRING="$CHANNEL"

    echo "[build] Building ($VARIANT)..."
    autobuild build -A "$AUTOBUILD_ADDRSIZE" -c "$VARIANT"

    echo "[build] Build complete."
fi
