XEMX for macOS: manual install

Drag each item to any folder you like. The shortcuts in this window go to the standard folders:

  XEMX.vst3       ->  VST3 folder, or your own VST3 folder (for example a custom folder set in Ableton)
  XEMX.component  ->  Components folder (AU), used by Logic, GarageBand and Ableton
  XEMX.app        ->  Applications (optional standalone)

IMPORTANT: this build is not notarized by Apple. After copying, run this once in Terminal,
with the path where you put each file, or the DAW will refuse to load it:

  xattr -cr "/Library/Audio/Plug-Ins/VST3/XEMX.vst3"
  xattr -cr "/Library/Audio/Plug-Ins/Components/XEMX.component"
  xattr -cr "/Applications/XEMX.app"

Prefer no Terminal? Use XEMX-Installer.pkg instead. It installs to the standard folders,
for all users or only for you, and needs no extra command.

Then in Ableton Live: Settings > Plug-Ins > turn on Audio Units / VST3 (System or Custom folder) > Rescan.
