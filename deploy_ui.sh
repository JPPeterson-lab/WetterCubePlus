#!/bin/bash
# Kopiert SquareLine Studio Export ins Projektverzeichnis
# SLS Exportziel: sls_export/
SRC="$(dirname "$0")/sls_export"
DST="$(dirname "$0")"
if [ ! -d "$SRC" ]; then
  echo "FEHLER: sls_export/ nicht gefunden."
  echo "SLS Export-Pfad auf: $(realpath "$SRC") setzen."
  exit 1
fi
cp "$SRC"/ui*.c "$DST/" 2>/dev/null && echo "ui*.c kopiert"
cp "$SRC"/ui*.h "$DST/" 2>/dev/null && echo "ui*.h kopiert"
# Nach dem Kopieren: LV_COLOR_16_SWAP-Block in ui.c auskommentieren
for f in "$DST"/ui.c "$DST"/ui_Screen*.c; do
  [ -f "$f" ] || continue
  sed -i '' 's|#if LV_COLOR_16_SWAP != 0|//#if LV_COLOR_16_SWAP != 0|g' "$f"
  sed -i '' 's|#error "LV_COLOR_16_SWAP|//#error "LV_COLOR_16_SWAP|g' "$f"
  sed -i '' 's|#endif // LV_COLOR_16_SWAP != 0|//#endif // LV_COLOR_16_SWAP != 0|g' "$f"
done
echo "Fertig. LV_COLOR_16_SWAP-Fehler deaktiviert."
