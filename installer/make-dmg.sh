#!/bin/bash
# Builds XEMX-macOS.dmg: the plug-ins plus shortcuts to the usual folders, for people who
# want to drag XEMX into a plug-in folder of their own choosing.
set -euo pipefail
ART=build/XEMX_artefacts/Release
STAGE=build/dmg/XEMX
rm -rf build/dmg; mkdir -p "$STAGE"
cp -R "$ART/VST3/XEMX.vst3" "$ART/AU/XEMX.component" "$ART/Standalone/XEMX.app" "$STAGE/"
ln -s /Library/Audio/Plug-Ins/VST3 "$STAGE/VST3 folder"
ln -s /Library/Audio/Plug-Ins/Components "$STAGE/Components folder (AU)"
ln -s /Applications "$STAGE/Applications"
cp installer/DMG-README.txt "$STAGE/Read me first.txt"
hdiutil create -volname "XEMX" -srcfolder "$STAGE" -fs HFS+ -format UDZO -ov XEMX-macOS.dmg
ls -la XEMX-macOS.dmg
