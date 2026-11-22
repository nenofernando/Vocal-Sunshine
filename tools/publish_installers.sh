#!/bin/bash
# Publishes the NF Vocal Sunshine installers for customers: the links in the licence e-mail
# (/download/nfvs-mac | nfvs-windows?k=<key>) download these two files from the R2 bucket mixa-downloads.
#   bash tools/publish_installers.sh          -> dry run: only shows what it would do
#   bash tools/publish_installers.sh --yes    -> publishes
# Needs (version = CMakeLists.txt line 2) on the Desktop:
#   "NF Vocal Sunshine <version>.dmg"        (bash Installer/macos/build_dmg.sh)
#   "NF Vocal Sunshine <version> Setup.exe"  (bash Installer/Windows/build_windows_on_mac.sh)
# It: 1) backs up what is live; 2) uploads NF-Vocal-Sunshine.dmg and NF-Vocal-Sunshine-Setup.exe; 3) checks the sizes.
# Plug-ins have no update box: customers download again from the e-mail link (or the owner re-sends the e-mail in /admin).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VER="$(grep -m1 -oE 'project\(NFVocalSunshine VERSION [0-9]+\.[0-9]+\.[0-9]+' "$ROOT/CMakeLists.txt" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
DIR="$HOME/Desktop"
DMG="$DIR/NF Vocal Sunshine $VER.dmg"; EXE="$DIR/NF Vocal Sunshine $VER Setup.exe"
BUCKET="mixa-downloads"
fail() { echo "NOT PUBLISHED: $1" >&2; exit 1; }
[ -n "$VER" ] || fail "could not read the version from CMakeLists.txt"
[ -f "$DMG" ] || fail "missing $DMG (bash Installer/macos/build_dmg.sh)"; [ -f "$EXE" ] || fail "missing $EXE (bash Installer/Windows/build_windows_on_mac.sh)"
export PATH="$HOME/.local/node-dl/node-v24.22.0-darwin-x64/bin:$PATH"; WR="$HOME/.local/wr/node_modules/.bin/wrangler"
echo "Version $VER"; ls -la "$DMG" "$EXE" | awk '{print "  " $5 "  " $9 " " $10 " " $11 " " $12 " " $13 " " $14}'
[ "${1:-}" = "--yes" ] || { echo "Dry run only. Add --yes to publish."; exit 0; }
BK="$HOME/Documents/NFVocalSunshineBackups/r2-before-$VER"; mkdir -p "$BK"
for o in NF-Vocal-Sunshine.dmg NF-Vocal-Sunshine-Setup.exe; do
  "$WR" r2 object get "$BUCKET/$o" --file "$BK/$o" --remote >/dev/null 2>&1 && echo "  backup: $o" || { echo "  (there was no $o online)"; rm -f "$BK/$o"; }
done
"$WR" r2 object put "$BUCKET/NF-Vocal-Sunshine.dmg" --file "$DMG" --content-type application/x-apple-diskimage --remote
"$WR" r2 object put "$BUCKET/NF-Vocal-Sunshine-Setup.exe" --file "$EXE" --content-type application/octet-stream --remote
# check: download both back and compare the sizes
for pair in "NF-Vocal-Sunshine.dmg|$DMG" "NF-Vocal-Sunshine-Setup.exe|$EXE"; do
  o="${pair%%|*}"; f="${pair#*|}"; t="$(mktemp)"
  "$WR" r2 object get "$BUCKET/$o" --file "$t" --remote >/dev/null 2>&1
  [ "$(wc -c < "$t")" = "$(wc -c < "$f")" ] && echo "  checked: $o ($(wc -c < "$t") bytes, same as the local file)" || fail "$o: size online differs from the local file"
  rm -f "$t"
done
echo "Published: NF Vocal Sunshine $VER (Mac and Windows) on the licence server."
