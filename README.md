# XEMX for macOS

A plugin version of the Korg Electribe EMX-1. It plays the patterns from `.emx` SmartMedia card files and puts every control where it sits on the original front panel.

- Formats: AU, VST3 and Standalone, universal binary (Apple Silicon + Intel), macOS 11+
- Loads `.emx` files with LOAD .EMX, by drag-and-drop, or from SHIFT + step 13. The card is saved inside the DAW project, and SAVE / WRITE export a new `.emx`.
- Drum samples come from your own EMX-1 sample folder or ZIP, picked with SAMPLES. By default it looks in `~/Downloads/ELECTRIBE`. No samples are included in this repo.
- Effects and output follow the service-manual block diagram: 3 FX slots with chain routing, audio-in thru, valve stage (TUBE GAIN), then master.

## Download a build

Every push to `main` runs **Actions → Build macOS**. Download the `XEMX-macOS` artifact from the run. It contains the plugins and `INSTALL-MAC.txt`.

The builds are ad-hoc signed, not notarized, so after copying the plugins run `xattr -cr` on them once (see INSTALL-MAC.txt).

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
- `.github/workflows/build-macos.yml`: CI build
