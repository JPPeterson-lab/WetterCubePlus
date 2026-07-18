#!/bin/bash
# WetterCubePlus Version Bump
# Verwendung: bash bump_version.sh 0.6.1-beta 0.7.0-beta

OLD=$1
NEW=$2

if [ -z "$OLD" ] || [ -z "$NEW" ]; then
  echo "Verwendung: bash bump_version.sh <alte-version> <neue-version>"
  echo "Beispiel:   bash bump_version.sh 0.6.1-beta 0.7.0-beta"
  exit 1
fi

# Shields.io braucht -- statt - vor "beta"
OLD_BADGE=$(echo "$OLD" | sed 's/-/--/g')
NEW_BADGE=$(echo "$NEW" | sed 's/-/--/g')

echo "Bumpe $OLD → $NEW ..."

# WetterCubePlus.ino
sed -i '' "s/FIRMWARE_VERSION \"$OLD\"/FIRMWARE_VERSION \"$NEW\"/" WetterCubePlus.ino

# docs/
sed -i '' "s/$OLD/$NEW/" docs/version.json
sed -i '' "s/$OLD/$NEW/" docs/manifest.json
sed -i '' "s/v$OLD/v$NEW/" docs/index.html

# README.md Badges (beide Sprachsektionen)
sed -i '' "s/Version-v$OLD_BADGE/Version-v$NEW_BADGE/g" README.md

echo "Ergebnis:"
grep "FIRMWARE_VERSION" WetterCubePlus.ino
grep "version" docs/version.json
grep "badge" docs/index.html | grep -o "v[0-9].*beta"
grep "Version-v" README.md | head -2
echo "Fertig. Jetzt bin exportieren, dann: bash deploy_and_release.sh $NEW"
