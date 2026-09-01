#!/bin/bash
set -e

# Self-contained build for Manikineko Online: FS-based viewer
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT/viewer"

export AUTOBUILD_VARIABLES_FILE="$ROOT/build-variables/variables"

if [ ! -d .venv ]; then
    python3 -m venv .venv
fi
source .venv/bin/activate

pip install --upgrade pip
pip install -r requirements.txt

# ReleaseOS: OpenSim, no KDU/FMOD, tests off, x86-64-v3 tuned
autobuild build -A 64 -c ReleaseOS -- -DVIEWER_CHANNEL:STRING=ManikinekoOnline -DLL_TESTS:BOOL=FALSE

echo "Build complete. Output is in the usual build/package directory."
