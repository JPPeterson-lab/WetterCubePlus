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
  if [ -f "$SRC/images.c" ]; then
    sed -i '' 's|#include "images/\(.*\)\.c"|#include "src/ui/images/\1.inc"|g' "$SRC/images.c"
    sed -i '' 's|#include "images/\(.*\)\.inc"|#include "src/ui/images/\1.inc"|g' "$SRC/images.c"
    echo "  images.c Include-Pfade auf src/ui/images/*.inc aktualisiert"
  fi
fi

# screens.c: PicoPixel exportiert Farben als lv_color_hex(0xffRRGGBB)
if [ -f "$SRC/screens.c" ]; then
  sed -i '' 's/lv_color_hex(0xff/lv_color_hex(0x/g' "$SRC/screens.c"
  echo "  screens.c: lv_color_hex 0xff-Prefix entfernt (PicoPixel ARGB→RGB Fix)"
fi

# ui.c: FADE_IN-Animation → NONE (Crash-Fix)
if [ -f "$SRC/ui.c" ]; then
  sed -i '' 's|lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false)|lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false)|g' "$SRC/ui.c"
  echo "  ui.c: FADE_IN-Animation → NONE (Crash-Fix)"
fi

# screens.c: Sonderzeichen ² (U+00B2) → ASCII (LVGL Montserrat-Font hat kein ²)
if [ -f "$SRC/screens.c" ]; then
  if grep -q 'mg/m²' "$SRC/screens.c"; then
    sed -i '' 's|mg/m²|ug/m3|g' "$SRC/screens.c"
    echo "  screens.c: Sonderzeichen mg/m² → ug/m3 ersetzt (Font-Fix)"
  fi
fi

# ── Verifikation: Prüfe ob alle vom .ino verwendeten Objekt-Namen noch vorhanden sind ──
echo ""
echo "Verifikation screens.h – erwartete Objekte:"
EXPECTED=(
  "labelbuttonforward"
  "labelbuttonforward_1"
  "labelbuttonforward_2"
  "labelbuttonforward_3"
  "labelbuttonforward_4"
  "labelbuttonforward_5"
  "labelbuttonbackward"
  "labelbuttonbackward_1"
  "labelbuttonbackward_2"
  "labelbuttonbackward_3"
  "labelbuttonbackward_4"
  "labelbuttonbackward_4_1"
  "labelbuttonbackward_5"
  "labelbuttonforward_5"
  "labelbuttonforward_6"
  "fc_settings"
  "screenbiowetter"
  "screenbiowetter2"
  "labelbuttonscreenhub"
  "labelbuttonscreenhubback"
  "labelbuttonscreenhub_1"
  "labelbuttonscreenhubback_1"
  "labelbuttonscreenhub_2"
  "labelbuttonscreenhubback_2"
  "labelbuttonhome"
  "labelbuttonhome_1"
  "labelbuttonhome_2"
  "labelbuttonhome_3"
  "labelbuttonhome_4"
  "labelbuttonhome_5"
  "labelbuttonhome_6"
  "labelbuttonhome_7"
  "labelbuttonhome_8"
  "arcaqi"
  "labelaqivalue"
  "labelaqistatus"
  "labelpm25value"
  "labelpm10value"
  "labelno2value"
  "labelo3value"
  "screenairquality"
  "screenwarnung"
  "screenwarnungpollen"
  "screenwarnkarte1"
  "screenwarnkarte2"
  "screenforecastpollen"
  "screenforecastpollenhour"
  "screensunmoon"
  "labeltime"
  "labeldatum"
  "labeltemp"
  "imagewetter"
  "labeluvindexvalue"
  "labelsunrisetime"
  "labelsundowntime"
  "labelwarntitel"
  "labelwarndetail"
  "labelwarndetail2"
  "labelwarnhint"
  "labelpollenwarntitel"
  "labelpollenwarnart"
  "labelpollenwarnhint"
  "labelhour1"
  "labelhour2"
  "labelhour3"
)

MISSING=0
if [ -f "$SRC/screens.h" ]; then
  for obj in "${EXPECTED[@]}"; do
    if ! grep -q "\b${obj}\b" "$SRC/screens.h"; then
      echo "  WARNUNG: '$obj' nicht in screens.h gefunden – Button/Objekt umbenannt?"
      MISSING=$((MISSING+1))
    fi
  done
  if [ $MISSING -eq 0 ]; then
    echo "  Alle erwarteten Objekte vorhanden."
  else
    echo ""
    echo "  !! $MISSING Objekte fehlen – WetterCubePlus.ino REG_CB / setLabel pruefen !!"
  fi
fi

echo ""
echo "Fertig. src/ui/ bereit fuer Arduino IDE."
