#!/bin/bash

# Configuration
PRODUCT_NAME="toolBoy Sh-equencer v1"
DMG_NAME="toolBoy_Sh-equencer_v1_Installer.dmg"
BUILD_DIR="build/shequencer_artefacts"
AU_PATH="$BUILD_DIR/AU/$PRODUCT_NAME.component"
VST3_PATH="$BUILD_DIR/VST3/$PRODUCT_NAME.vst3"
STAGING_DIR="build/dmg_staging"

# Signing Identity - Try to find it or use environment variable
if [ -z "$DEV_ID_APP" ]; then
    echo "Searching for Developer ID Application identity..."
    DEV_ID_APP=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | cut -d '"' -f 2)
fi

if [ -z "$DEV_ID_APP" ]; then
    echo "Error: No Developer ID Application identity found."
    echo "Please set DEV_ID_APP environment variable or ensure you have a valid certificate in your keychain."
    exit 1
fi

echo "Using Identity: $DEV_ID_APP"

# 1. Codesign the artifacts
echo "Codesigning AU..."
codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$AU_PATH"
echo "Codesigning VST3..."
codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$VST3_PATH"

# 2. Prepare Staging Directory
echo "Preparing DMG staging area..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

cp -R "$AU_PATH" "$STAGING_DIR/"
cp -R "$VST3_PATH" "$STAGING_DIR/"

# 3. Create Symlinks for Easy Install
echo "Creating symlinks..."
ln -s "/Library/Audio/Plug-Ins/Components" "$STAGING_DIR/Components"
ln -s "/Library/Audio/Plug-Ins/VST3" "$STAGING_DIR/VST3"

# 4. Create DMG
echo "Creating DMG..."
rm -f "$DMG_NAME"
hdiutil create -volname "$PRODUCT_NAME Installer" -srcfolder "$STAGING_DIR" -ov -format UDZO "$DMG_NAME"

# 5. Sign DMG
echo "Signing DMG..."
codesign --force --sign "$DEV_ID_APP" --timestamp "$DMG_NAME"

# 6. Notarize DMG
# Check for NOTARY_PROFILE env var, or try to find a common one, or ask user.
if [ -z "$NOTARY_PROFILE" ]; then
    echo "NOTARY_PROFILE not set. Checking for stored credentials..."
    # This is a bit of a guess, but we can try to list profiles or just warn.
    # Since we can't interactively ask, we'll skip if not set.
    echo "Skipping notarization because NOTARY_PROFILE environment variable is not set."
    echo "To notarize, run: xcrun notarytool store-credentials \"MyProfile\" ..."
    echo "Then export NOTARY_PROFILE=\"MyProfile\" and run this script again."
else
    echo "Notarizing DMG using profile: $NOTARY_PROFILE"
    xcrun notarytool submit "$DMG_NAME" --keychain-profile "$NOTARY_PROFILE" --wait
    
    if [ $? -eq 0 ]; then
        echo "Notarization successful. Stapling ticket..."
        xcrun stapler staple "$DMG_NAME"
    else
        echo "Notarization failed."
        exit 1
    fi
fi

echo "Done! Installer created at $DMG_NAME"
