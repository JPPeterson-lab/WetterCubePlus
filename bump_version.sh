#!/bin/bash
# Setzt die Firmware-Version an allen Stellen und prueft die Konsistenz.
#   bash bump_version.sh 1.1.2             setzt die Version ueberall und prueft danach
#   bash bump_version.sh 1.1.1 1.1.2       alter Aufruf <alt> <neu> geht weiterhin (alt wird ignoriert)
#   bash bump_version.sh --check           prueft nur (inkl. exportierter docs/firmware/firmware.bin)
#
# Stellen: FIRMWARE_VERSION in WetterCubePlus.ino (daraus kommen Menue-Anzeige und WebUI),
#          docs/version.json, docs/manifest.json, Badge in docs/index.html, README-Badges (DE + EN)
cd "$(dirname "$0")" || exit 1
INO="WetterCubePlus.ino"

if [ "$1" != "--check" ]; then
  V="${2:-$1}"   # bei zwei Argumenten (<alt> <neu>) zaehlt das letzte
  if ! [[ "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9]+)?$ ]]; then
    echo "Aufruf: bash $0 X.Y.Z   (z. B. 1.1.2)  oder  bash $0 --check"
    exit 1
  fi
  sed -i '' "s/#define FIRMWARE_VERSION \".*\"/#define FIRMWARE_VERSION \"$V\"/" "$INO"
  printf '{"version": "%s"}\n' "$V" > docs/version.json
  sed -i '' "s/\"version\": \"[^\"]*\"/\"version\": \"$V\"/" docs/manifest.json
  sed -i '' "s|\(<span class=\"badge\">v\)[^<]*\(</span>\)|\1$V\2|" docs/index.html
  VB="${V//-/--}"   # shields.io: ein Bindestrich im Text wird als -- geschrieben
  sed -i '' "s/Version-v[0-9.]*\(--[A-Za-z0-9]*\)\{0,1\}-blue/Version-v$VB-blue/g" README.md
fi

# ── Pruefung ────────────────────────────────────────────────────────────────
SOLL=$(grep -m1 '#define FIRMWARE_VERSION' "$INO" | sed 's/.*"\(.*\)".*/\1/')
FEHLER=0
pruefe() {   # Name, gefundener Wert
  if [ "$2" = "$SOLL" ]; then printf "  OK       %-34s %s\n" "$1" "$2"
  else printf "  FALSCH   %-34s %s (erwartet %s)\n" "$1" "${2:-nicht gefunden}" "$SOLL"; FEHLER=1; fi
}

echo "Version laut $INO: $SOLL"
pruefe "docs/version.json"   "$(sed 's/.*"version": *"\([^"]*\)".*/\1/' docs/version.json)"
pruefe "docs/manifest.json"  "$(grep -m1 '"version"' docs/manifest.json | sed 's/.*"version": *"\([^"]*\)".*/\1/')"
pruefe "docs/index.html"     "$(grep -m1 'class="badge"' docs/index.html | sed 's/.*class="badge">v\([^<]*\)<.*/\1/')"
n=0
while IFS= read -r b; do
  n=$((n+1)); pruefe "README.md Badge $n" "$(echo "$b" | sed 's/.*Version-v\(.*\)-blue.*/\1/; s/--/-/')"
done < <(grep 'img.shields.io/badge/Version-' README.md)
[ "$n" -ne 2 ] && { echo "  FALSCH   README.md: $n statt 2 Version-Badges"; FEHLER=1; }

# Exportiertes Binary: muss die Version enthalten, sonst wurde in der IDE nicht neu exportiert
BIN="docs/firmware/firmware.bin"
if [ -f "$BIN" ]; then
  BV=$(strings "$BIN" | grep -m1 'WetterCubePlus\] Start v' | sed 's/.*Start v//')
  if [ "$BV" = "$SOLL" ]; then printf "  OK       %-34s %s (%s Bytes)\n" "$BIN" "$BV" "$(stat -f%z "$BIN")"
  else printf "  VERALTET %-34s %s, erwartet %s: in der IDE neu exportieren!\n" "$BIN" "${BV:-keine Version gefunden}" "$SOLL"; FEHLER=1; fi
fi

[ "$FEHLER" -eq 0 ] && echo "Alles konsistent." || echo "Es gibt Abweichungen (siehe oben)."
exit $FEHLER
