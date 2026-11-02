#!/bin/bash
# Builds XEMX-Installer.pkg from the Release artefacts.
# Optional env: INSTALLER_IDENTITY ("Developer ID Installer: ...") to sign the package.
set -euo pipefail
VERSION="${VERSION:-0.1.0}"
ART=build/XEMX_artefacts/Release
WORK=build/pkg
rm -rf "$WORK"; mkdir -p "$WORK"/{vst3,au,app,pkgs}

cp -R "$ART/VST3/XEMX.vst3" "$WORK/vst3/"
cp -R "$ART/AU/XEMX.component" "$WORK/au/"
cp -R "$ART/Standalone/XEMX.app" "$WORK/app/"

# One component package per format, so people can untick what they don't need.
pkgbuild --root "$WORK/vst3" --identifier com.xemx.xemx.vst3 --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/pkgs/xemx-vst3.pkg"
pkgbuild --root "$WORK/au" --identifier com.xemx.xemx.au --version "$VERSION" \
         --scripts installer/scripts --install-location "/Library/Audio/Plug-Ins/Components" "$WORK/pkgs/xemx-au.pkg"
pkgbuild --root "$WORK/app" --identifier com.xemx.xemx.app --version "$VERSION" \
         --install-location "/Applications" "$WORK/pkgs/xemx-app.pkg"

SIGN=()
if [ -n "${INSTALLER_IDENTITY:-}" ]; then SIGN=(--sign "$INSTALLER_IDENTITY"); fi

productbuild --distribution installer/distribution.xml --resources installer/resources \
             --package-path "$WORK/pkgs" "${SIGN[@]}" "XEMX-Installer.pkg"
ls -la XEMX-Installer.pkg
