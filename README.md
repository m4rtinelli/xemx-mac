# XEMX for macOS

<p align="center">
  <a href="https://github.com/m4rtinelli/xemx-mac/releases/latest/download/XEMX-Installer.pkg"><img alt="Download installer (.pkg)" src="https://img.shields.io/badge/Download-Installer%20(.pkg)-ff5a1f?style=for-the-badge&logo=apple&logoColor=white"></a>
  &nbsp;
  <a href="https://github.com/m4rtinelli/xemx-mac/releases/latest/download/XEMX-macOS.dmg"><img alt="Download disk image (.dmg)" src="https://img.shields.io/badge/Download-Disk%20image%20(.dmg)-30343c?style=for-the-badge&logo=apple&logoColor=white"></a>
</p>

A plugin version of the Korg Electribe EMX-1. It plays the patterns from `.emx` SmartMedia card files and puts every control where it sits on the original front panel.

- Formats: AU, VST3 and Standalone, universal binary (Apple Silicon + Intel), macOS 11+
- Loads `.emx` files with LOAD .EMX, by drag-and-drop, or from SHIFT + step 13. The card is saved inside the DAW project, and SAVE / WRITE export a new `.emx`.
- Drum samples come from your own EMX-1 sample folder or ZIP, picked with SAMPLES. By default it looks in `~/Downloads/ELECTRIBE`. No samples are included in this repo.
- Effects and output follow the service-manual block diagram: 3 FX slots with chain routing, audio-in thru, valve stage (TUBE GAIN), then master.

## Install

Two downloads, both universal (Apple Silicon + Intel), macOS 11 or later:

| | What it does | Pick this if |
|---|---|---|
| **Installer (.pkg)** | Double-click and follow the steps. Under **Install for me only** it goes to `~/Library/Audio/Plug-Ins`; under **Install for all users** to `/Library/Audio/Plug-Ins`. **Customize** picks VST3, AU and/or the app. | You want the easy way. No Terminal needed. |
| **Disk image (.dmg)** | Opens a window with `XEMX.vst3`, `XEMX.component`, `XEMX.app` and shortcuts to the standard folders. Drag each one wherever you want, including your own VST3 folder. | You keep plug-ins in a custom folder. |

**First launch:** the downloads are not notarized yet, so macOS shows an "unidentified developer" warning. Open **System Settings → Privacy & Security** and click **Open Anyway**. With the .dmg you also need to run `xattr -cr` once on the copied plug-ins (the exact commands are in *Read me first.txt* inside the image).

**In Ableton Live:** Settings → Plug-Ins → turn on Audio Units and/or VST3 (system or custom folder) → Rescan.

## Releases and signing

- Push a tag such as `v0.1.0` to publish a GitHub Release with both downloads attached. The buttons above always point to the latest release.
- To remove the macOS warning, add these repository secrets from an Apple Developer account. CI then signs with Developer ID, notarizes and staples automatically:
  `MACOS_CERTS_P12` (base64 .p12 with the Developer ID Application + Installer certificates), `MACOS_CERTS_PASSWORD`, `MACOS_APP_IDENTITY`, `MACOS_INSTALLER_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`.

## Build on a Mac

Requires Xcode and CMake 3.22 or later.

```
cmake -S . -B build -G Xcode
cmake --build build --config Release
```

## Layout

- `Source/`: engine, EMX file model, panel UI (shared with the Windows repo)
- `factory/FACTORY.EMX`: factory patterns built into the plugin
- `docs/EMX_FORMAT.md`: the decoded `.emx` format
- `installer/`: .pkg installer (distribution, welcome and finish pages)
- `.github/workflows/build-macos.yml`: CI build, installer, optional notarization
