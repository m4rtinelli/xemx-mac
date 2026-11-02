# XEMX for macOS

A plugin version of the Korg Electribe EMX-1. It plays the patterns from `.emx` SmartMedia card files and puts every control where it sits on the original front panel.

- Formats: AU, VST3 and Standalone, universal binary (Apple Silicon + Intel), macOS 11+
- Loads `.emx` files with LOAD .EMX, by drag-and-drop, or from SHIFT + step 13. The card is saved inside the DAW project, and SAVE / WRITE export a new `.emx`.
- Drum samples come from your own EMX-1 sample folder or ZIP, picked with SAMPLES. By default it looks in `~/Downloads/ELECTRIBE`. No samples are included in this repo.
- Effects and output follow the service-manual block diagram: 3 FX slots with chain routing, audio-in thru, valve stage (TUBE GAIN), then master.

## Install

Download **XEMX-Installer.pkg** from the latest Release (or from the `XEMX-Installer` artifact of an Actions run), double-click it and follow the steps. It installs the VST3 to `/Library/Audio/Plug-Ins/VST3`, the Audio Unit to `/Library/Audio/Plug-Ins/Components` and the app to `/Applications`. **Customize** lets you pick only some of them.

The installer is not notarized yet. The first time, macOS shows an "unidentified developer" warning: open System Settings → Privacy & Security and click **Open Anyway**.

## Releases and signing

- Push a tag such as `v0.1.0` to publish a GitHub Release with the installer attached.
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
