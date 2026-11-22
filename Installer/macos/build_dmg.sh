#!/bin/bash
# NF Vocal Sunshine - macOS installer builder (NF Audio Tools by Nenno Fernando).
#
# Builds NF Vocal Sunshine as VST3 + AU, universal (arm64 + x86_64), on THIS Mac, with the access-key lock ON,
# wraps them into a standard macOS installer (.pkg: welcome, read-me, format options, conclusion) and puts it in a DMG.
# Nothing is uploaded anywhere. No AAX.
#
# Usage:   bash Installer/macos/build_dmg.sh
# Result:  ~/Desktop/NF Vocal Sunshine <version>.dmg
#
# No Apple Developer account: VST3/AU are ad-hoc signed and the .pkg is unsigned
# (on another Mac: right-click the .pkg > Open the first time, or System Settings > Privacy & Security > Open Anyway).
#
# Optional environment variables:
#   JUCE_SRC=/path/to/JUCE   JUCE checkout (default ~/JUCE)
#   OUT_DIR=/folder          where the .dmg is written (default ~/Desktop)
#   ARCHS="arm64"            build only this architecture (default arm64;x86_64)
#   SIGN_ID="..."            code-sign identity (default "-" = ad-hoc)
# The version comes from CMakeLists.txt (project(NFVocalSunshine VERSION X.Y.Z ...)).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERSION="$(grep -m1 -oE 'project\(NFVocalSunshine VERSION [0-9]+\.[0-9]+\.[0-9]+' "$REPO_ROOT/CMakeLists.txt" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
[ -n "$VERSION" ] || { echo "Could not read the version from CMakeLists.txt" >&2; exit 1; }
JUCE_SRC="${JUCE_SRC:-$HOME/JUCE}"
[ -f "$JUCE_SRC/CMakeLists.txt" ] || { echo "JUCE not found at $JUCE_SRC (set JUCE_SRC=...)" >&2; exit 1; }

PRODUCT="NF Vocal Sunshine"
ID_BASE="audio.nf.vocalsunshine.installer"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-installer}"
WORK="$REPO_ROOT/build-installer-work"
OUT_DIR="${OUT_DIR:-$HOME/Desktop}"
SIGN_ID="${SIGN_ID:--}"
PKG_NAME="Install $PRODUCT $VERSION.pkg"
VOLNAME="$PRODUCT $VERSION"
DMG_PATH="$OUT_DIR/$PRODUCT $VERSION.dmg"
YEAR="$(date +%Y)"

command -v cmake >/dev/null || { echo "cmake not found (brew install cmake)" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja not found (brew install ninja)" >&2; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools not found" >&2; exit 1; }

echo "==> Building $PRODUCT $VERSION (VST3 + AU, access key ON)"
ARCHS="${ARCHS:-arm64;x86_64}"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=$ARCHS" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DNFVocalSunshine_LICENSE=ON -DNFVocalSunshine_BUILD_TOOLS=OFF "-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_SRC"
cmake --build "$BUILD_DIR" --config Release --target NFVocalSunshine_VST3 NFVocalSunshine_AU

find_one() { find "$BUILD_DIR" -name "$1" -type d | head -n 1; }
C_VST3="$(find_one "$PRODUCT.vst3")";   C_AU="$(find_one "$PRODUCT.component")"
for p in "$C_VST3" "$C_AU"; do [ -d "$p" ] || { echo "Built plug-in missing under $BUILD_DIR" >&2; exit 1; }; done

echo "==> Staging and signing (identity: $SIGN_ID)"
rm -rf "$WORK"; mkdir -p "$WORK/root/vst3" "$WORK/root/au" "$WORK/pkgs" "$WORK/dmg" "$WORK/resources"
cp -R "$C_VST3" "$WORK/root/vst3/"
cp -R "$C_AU" "$WORK/root/au/"
# the access-key check must be compiled in, in BOTH formats (never ship a build without it)
for bin in "$WORK/root/vst3/$PRODUCT.vst3/Contents/MacOS/$PRODUCT" "$WORK/root/au/$PRODUCT.component/Contents/MacOS/$PRODUCT"; do
  [ "$(strings "$bin" | grep -c "/activate")" -gt 0 ] || { echo "Access-key code missing from $bin: refusing to make the installer" >&2; exit 1; }
  ARCHS_BUILT="$(lipo -archs "$bin")"
  [ "$ARCHS" != "arm64;x86_64" ] || { echo "$ARCHS_BUILT" | grep -q arm64 && echo "$ARCHS_BUILT" | grep -q x86_64; } || { echo "Not universal: $ARCHS_BUILT" >&2; exit 1; }
done
for b in "$WORK"/root/vst3/*.vst3 "$WORK"/root/au/*.component; do codesign --force --deep --sign "$SIGN_ID" "$b"; done

make_component() {   # name, install location
  local root="$WORK/root/$1" plist="$WORK/$1.plist"
  pkgbuild --analyze --root "$root" "$plist" >/dev/null
  /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$plist" >/dev/null 2>&1 || true
  /usr/libexec/PlistBuddy -c "Set :1:BundleIsRelocatable false" "$plist" >/dev/null 2>&1 || true
  pkgbuild --root "$root" --component-plist "$plist" --identifier "$ID_BASE.$1" --version "$VERSION" \
           --install-location "$2" "$WORK/pkgs/NFVocalSunshine-$(echo "$1" | tr '[:lower:]' '[:upper:]').pkg" >/dev/null
}
echo "==> Building installer packages"
make_component vst3 "/Library/Audio/Plug-Ins/VST3"
make_component au   "/Library/Audio/Plug-Ins/Components"

style='<style>body{font-family:-apple-system,Helvetica,Arial,sans-serif;font-size:13px;line-height:1.5}h1{font-size:20px;margin:0 0 4px}h2{font-size:14px;margin:16px 0 4px}.by{color:#555;margin:0 0 14px}td{padding:2px 12px 2px 0;vertical-align:top}</style>'
cat > "$WORK/resources/Welcome.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>$PRODUCT $VERSION</h1>
<p class="by">NF Audio Tools by Nenno Fernando</p>
<p>Welcome to the $PRODUCT installer.</p>
<p>$PRODUCT is a complete vocal processor in 12 stages: clean-up, EQ, compressor, de-esser, colour, doubling, harmonies with automatic key, reverb, delay and a finishing limiter.</p>
<p>On the <b>Customize</b> step you can choose the formats to install:</p>
<ul><li>VST3</li><li>Audio Unit (AU)</li></ul>
<p>Click <b>Continue</b> to proceed.</p>
</body></html>
EOF
cat > "$WORK/resources/ReadMe.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>$PRODUCT - Read Me</h1>
<p class="by">NF Audio Tools by Nenno Fernando &middot; Version $VERSION</p>
<h2>Installed plug-ins</h2>
<table>
<tr><td><b>VST3</b></td><td>/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3</td></tr>
<tr><td><b>Audio Unit</b></td><td>/Library/Audio/Plug-Ins/Components/$PRODUCT.component</td></tr>
</table>
<h2>How to use</h2>
<p>Insert $PRODUCT on a vocal track. Pick a stage in the list on the left, turn its big knob and choose its style. The LED next to a stage switches it on or off. Every stage has its own low cut, high cut, output and mix; the FINISH stage carries the master controls.</p>
<h2>Access key</h2>
<p>$PRODUCT needs an access key. The first time you open the plug-in, type the short key you received after your purchase (it looks like NFVS-XXXX-XXXX-XXXX-XXXX) and click Activate (internet needed once). One key works on up to 2 computers.</p>
<h2>System requirements</h2>
<p>macOS 11 or later. Native for Apple Silicon and Intel (universal). A host that supports VST3 or Audio Units.</p>
<h2>After installing</h2>
<p>Restart your DAW and rescan plug-ins if they do not show up.</p>
<p>$PRODUCT &copy; $YEAR NF Audio Tools by Nenno Fernando. All rights reserved.</p>
</body></html>
EOF
cat > "$WORK/resources/Conclusion.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>Installation complete</h1>
<p class="by">NF Audio Tools by Nenno Fernando</p>
<p>$PRODUCT $VERSION was installed successfully.</p>
<p>Restart your DAW and rescan your plug-ins. Type your access key when the plug-in asks for it.</p>
<p>Thank you for choosing NF Audio Tools.</p>
</body></html>
EOF
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$PRODUCT $VERSION</title>
  <organization>audio.nf</organization>
  <welcome file="Welcome.html" mime-type="text/html"/>
  <readme file="ReadMe.html" mime-type="text/html"/>
  <conclusion file="Conclusion.html" mime-type="text/html"/>
  <background file="logo.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_anywhere="false" enable_localSystem="true"/>
  <choices-outline><line choice="vst3"/><line choice="au"/></choices-outline>
  <choice id="vst3" visible="true" start_selected="true" title="VST3" description="Installs $PRODUCT.vst3 in /Library/Audio/Plug-Ins/VST3.">
    <pkg-ref id="$ID_BASE.vst3"/>
  </choice>
  <choice id="au" visible="true" start_selected="true" title="Audio Unit (AU)" description="Installs $PRODUCT.component in /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand).">
    <pkg-ref id="$ID_BASE.au"/>
  </choice>
  <pkg-ref id="$ID_BASE.vst3" version="$VERSION" onConclusion="none">NFVocalSunshine-VST3.pkg</pkg-ref>
  <pkg-ref id="$ID_BASE.au" version="$VERSION" onConclusion="none">NFVocalSunshine-AU.pkg</pkg-ref>
</installer-gui-script>
EOF
sips -Z 260 "$REPO_ROOT/Assets/NF_Audio_Tools_Logo_MAIN_800.png" --out "$WORK/resources/logo.png" >/dev/null   # small logo for the installer background
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" --package-path "$WORK/pkgs" "$WORK/dmg/$PKG_NAME" >/dev/null

echo "==> Creating DMG"
mkdir -p "$OUT_DIR"; RW="$WORK/rw.dmg"
hdiutil create -volname "$VOLNAME" -srcfolder "$WORK/dmg" -fs HFS+ -format UDRW -ov "$RW" >/dev/null
MOUNT="$(hdiutil attach -readwrite -noverify -noautoopen "$RW" | grep -o '/Volumes/.*' | head -n 1)"
if [ -n "$MOUNT" ]; then
  osascript >/dev/null 2>&1 <<OSA || echo "(Finder window layout skipped)"
tell application "Finder"
  tell disk "$VOLNAME"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {200, 140, 720, 400}
    set viewOptions to the icon view options of container window
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 128
    set position of item "$PKG_NAME" of container window to {260, 120}
    update without registering applications
    delay 2
    close
  end tell
end tell
OSA
  bless --folder "$MOUNT" --openfolder "$MOUNT" >/dev/null 2>&1 || true
  sync; hdiutil detach "$MOUNT" >/dev/null || hdiutil detach "$MOUNT" -force >/dev/null
fi
rm -f "$DMG_PATH"
hdiutil convert "$RW" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH" >/dev/null
echo; echo "Done: $DMG_PATH"
