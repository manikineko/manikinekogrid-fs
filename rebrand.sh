#!/bin/bash
set -e

# Run from the repo root
ROOT="$(cd "$(dirname "$0")" && pwd)"
LOGO="$ROOT/branding/logo.png"

if [ ! -f "$LOGO" ]; then
    echo "branding/logo.png not found" >&2
    exit 1
fi

cd "$ROOT"

echo "==> Replacing Firestorm icons with Manikineko logo..."

# PNG/BMP size-matched replacement
find viewer -type f \( -name 'firestorm_*.png' -o -name 'firestorm_*.bmp' \) | while read -r f; do
    size="$(identify -format '%w %h' "$f" | head -n1)"
    w="$(echo "$size" | cut -d' ' -f1)"
    h="$(echo "$size" | cut -d' ' -f2)"
    if [ -n "$w" ] && [ -n "$h" ]; then
        convert "$LOGO" -resize "${w}x${h}^" -gravity center -extent "${w}x${h}" -background transparent "$f"
    fi
done

# Multi-resolution .ico files
find viewer -type f -name 'firestorm_icon*.ico' | while read -r f; do
    convert "$LOGO" -resize 256x256 -define icon:auto-resize=16,32,48,128,256 "$f"
done

echo "==> Rebranding user-facing strings..."

# Core English strings
sed -i 's|<string name="APP_NAME">Firestorm</string>|<string name="APP_NAME">Manikineko Online</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml
sed -i 's|<string name="CAPITALIZED_APP_NAME">FIRESTORM</string>|<string name="CAPITALIZED_APP_NAME">MANIKINEKO ONLINE</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml
sed -i 's|<string name="SUPPORT_SITE">Firestorm Support Portal</string>|<string name="SUPPORT_SITE">Manikineko Online Support</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml
sed -i 's|<string name="DOWNLOAD_URL">.*</string>|<string name="DOWNLOAD_URL">https://manikineko.nl</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml
sed -i 's|<string name="ViewerDownloadURL">.*</string>|<string name="ViewerDownloadURL">https://manikineko.nl</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml
sed -i 's|<string name="create_account_url">.*</string>|<string name="create_account_url">https://manikineko.nl</string>|g' viewer/indra/newview/skins/default/xui/en/strings.xml

# Skin name in skins.xml
sed -i 's|<string>Firestorm</string>|<string>Manikineko Online</string>|g' viewer/indra/newview/skins/skins.xml

# Inventory folder label and root folder name
sed -i 's/new ViewerFolderEntry("Firestorm"/new ViewerFolderEntry("Manikineko Online"/g' viewer/indra/newview/llviewerfoldertype.cpp
sed -i 's|"#Firestorm"|"#Manikineko"|g' viewer/indra/newview/llinventoryfunctions.h

# Autobuild metadata (leave copyright intact, only the display name)
sed -i 's/Firestorm Viewer/Manikineko Online: FS-based viewer/g' viewer/autobuild.xml

echo "==> Rebrand complete."
