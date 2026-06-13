#!/bin/bash
# PicoPixel UI-Export vorbereiten
# PicoPixel Exportziel: src/ui/  (Arduino IDE kompiliert src/ automatisch rekursiv)
SRC="$(dirname "$0")/src/ui"

if [ ! -d "$SRC" ]; then
  echo "FEHLER: src/ui/ nicht gefunden."
  echo "PicoPixel Export-Pfad auf: $(realpath "$SRC") setzen."
  exit 1
fi

# PicoPixel generiert "lvgl/lvgl.h" – Arduino kennt nur <lvgl.h>
find "$SRC" -name "*.c" -o -name "*.h" | while read f; do
  if grep -q '"lvgl/lvgl.h"' "$f"; then
    sed -i '' 's|#include "lvgl/lvgl.h"|#include <lvgl.h>|g' "$f"
    echo "  lvgl-Include gepatcht: $(basename $f)"
  fi
done

# images/*.c werden von images.c per #include eingebunden UND von Arduino direkt kompiliert
# → doppelte Definitionen; Lösung: zu .inc umbenennen
if [ -d "$SRC/images" ]; then
  for f in "$SRC/images"/*.c; do
    [ -f "$f" ] || continue
    base="${f%.c}"
    mv "$f" "${base}.inc"
    echo "  Image umbenannt: $(basename $f) → $(basename ${base}.inc)"
  done
  # images.c Includes anpassen: relativer Pfad → src/ui/images/ (Arduino kompiliert vom Sketch-Root)
  if [ -f "$SRC/images.c" ]; then
    sed -i '' 's|#include "images/\(.*\)\.c"|#include "src/ui/images/\1.inc"|g' "$SRC/images.c"
    sed -i '' 's|#include "images/\(.*\)\.inc"|#include "src/ui/images/\1.inc"|g' "$SRC/images.c"
    echo "  images.c Include-Pfade auf src/ui/images/*.inc aktualisiert"
  fi
fi

# screens.c: PicoPixel exportiert Farben als lv_color_hex(0xffRRGGBB) – LVGL ignoriert
# den Alpha-Byte und liest 0xFF als Rot-Kanal → alle Farben falsch (Schwarz wird Rot/Blau).
if [ -f "$SRC/screens.c" ]; then
  sed -i '' 's/lv_color_hex(0xff/lv_color_hex(0x/g' "$SRC/screens.c"
  echo "  screens.c: lv_color_hex 0xff-Prefix entfernt (PicoPixel ARGB→RGB Fix)"
fi

# ui.c: FADE_IN-Animation entfernen – führt zu d->scr_to_load=NULL-Crash
# wenn lv_scr_load() aufgerufen wird bevor 200ms Animation abläuft
if [ -f "$SRC/ui.c" ]; then
  sed -i '' 's|lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false)|lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false)|g' "$SRC/ui.c"
  echo "  ui.c: FADE_IN-Animation → NONE (Crash-Fix)"
fi

echo ""
echo "Fertig. src/ui/ bereit für Arduino IDE."
