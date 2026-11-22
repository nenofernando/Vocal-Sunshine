#!/bin/bash
# NF Vocal Sunshine - Windows installer, built on the Mac (NF Audio Tools by Nenno Fernando).
# Cross-compiles the VST3 for Windows x64 (clang-cl + the xwin SDK) and wraps it in a self-contained installer .exe
# (Installer/Windows/stub). No Windows, GitHub Actions or Inno Setup needed. Nothing is uploaded anywhere.
#
# Needs: ~/Documents/xwin-sdk (xwin SDK + clang-cl.cmake), LLVM (brew), ninja, and a JUCE checkout (default ~/JUCE).
# Usage:   bash Installer/Windows/build_windows_on_mac.sh
# Result:  ~/Desktop/NF Vocal Sunshine <version> Setup.exe
# Optional: JUCE_SRC=/path/to/JUCE   OUT_DIR=/folder
# The version comes from CMakeLists.txt (project(NFVocalSunshine VERSION X.Y.Z ...)).
# The installer was never run on a real Windows machine from here: test it on Windows before selling.
set -euo pipefail
cd "$(dirname "$0")/../.."
TC="$HOME/Documents/xwin-sdk/clang-cl.cmake"
JUCE_SRC="${JUCE_SRC:-$HOME/JUCE}"
OUT_DIR="${OUT_DIR:-$HOME/Desktop}"
[ -f "$TC" ] || { echo "Windows toolchain not found: $TC" >&2; exit 1; }
[ -f "$JUCE_SRC/CMakeLists.txt" ] || { echo "JUCE not found at $JUCE_SRC (set JUCE_SRC=...)" >&2; exit 1; }
VER="$(grep -m1 -oE 'project\(NFVocalSunshine VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
[ -n "$VER" ] || { echo "Could not read the version from CMakeLists.txt" >&2; exit 1; }

echo "==> Cross-compiling NF Vocal Sunshine $VER (VST3, Windows x64)"
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DCMAKE_OSX_ARCHITECTURES= "-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_SRC"
cmake --build build-win --target NFVocalSunshine_VST3
DLL="$PWD/build-win/NFVocalSunshine_artefacts/Release/VST3/NF Vocal Sunshine.vst3/Contents/x86_64-win/NF Vocal Sunshine.vst3"
[ -f "$DLL" ] || { echo "VST3 DLL not built: $DLL" >&2; exit 1; }
file "$DLL" | grep -q "PE32+" || { echo "The built file is not a 64-bit Windows binary" >&2; exit 1; }

echo "==> Building the installer"
rm -rf build-win-installer
cmake -S Installer/Windows/stub -B build-win-installer -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded "-DNFVS_PAYLOAD_DLL=$DLL" -DNFVS_VERSION="$VER" -DCMAKE_OSX_ARCHITECTURES=
cmake --build build-win-installer
mkdir -p "$OUT_DIR"
OUT="$OUT_DIR/NF Vocal Sunshine $VER Setup.exe"
cp build-win-installer/NFVSInstaller.exe "$OUT"
echo; echo "Done: $OUT"
