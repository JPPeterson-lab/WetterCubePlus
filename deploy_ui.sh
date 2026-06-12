#!/bin/bash
# Kopiert PicoPixel-Export (src/ui/) ins Projektverzeichnis
# PicoPixel Exportziel: src/ui/
SRC="$(dirname "$0")/src/ui"
DST="$(dirname "$0")"

if [ ! -d "$SRC" ]; then
  echo "FEHLER: src/ui/ nicht gefunden."
  echo "PicoPixel Export-Pfad auf: $(realpath "$SRC") setzen."
  exit 1
fi

# Zielordner anlegen
mkdir -p "$DST/ui/fonts"

# Alle UI-Dateien kopieren
cp "$SRC"/*.c  "$DST/ui/" 2>/dev/null && echo "*.c kopiert"
cp "$SRC"/*.h  "$DST/ui/" 2>/dev/null && echo "*.h kopiert"
cp "$SRC"/fonts/*.c "$DST/ui/fonts/" 2>/dev/null && echo "fonts/*.c kopiert"
cp "$SRC"/fonts/*.h "$DST/ui/fonts/" 2>/dev/null && echo "fonts/*.h kopiert"

# Images-Unterordner wenn vorhanden
if [ -d "$SRC/images" ]; then
  mkdir -p "$DST/ui/images"
  cp "$SRC/images"/* "$DST/ui/images/" 2>/dev/null && echo "images/* kopiert"
fi

# PicoPixel generiert "lvgl/lvgl.h" – Arduino kennt nur <lvgl.h>
find "$DST/ui" -name "*.c" -o -name "*.h" | xargs grep -l '"lvgl/lvgl.h"' 2>/dev/null | while read f; do
  sed -i '' 's|#include "lvgl/lvgl.h"|#include <lvgl.h>|g' "$f"
  echo "  lvgl-Include gepatcht: $(basename $f)"
done

echo ""
echo "Fertig. PicoPixel UI in ui/ bereit."
