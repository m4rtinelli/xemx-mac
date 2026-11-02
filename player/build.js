// Builds the player: embeds the patterns+songs part of the factory file and the wave names.
// Usage: node player/build.js factory/FACTORY.EMX factory/waves.txt [samplesDir]
//   without samplesDir -> player/emx-player.html (published; viewer loads samples)
//   with samplesDir    -> player/emx-player-local.html (samples embedded as default; keep local)
const fs = require("fs"), path = require("path");
const [emxFile, wavesFile, samplesDir] = process.argv.slice(2);
const SONGS_END = 0x130000 + 64 * 0x210; // everything the player reads
const emx = fs.readFileSync(emxFile).subarray(0, SONGS_END);
const waves = fs.readFileSync(wavesFile, "utf8").split(/\r?\n/).filter(Boolean);
if (waves.length !== 207) throw new Error(`expected 207 wave names, got ${waves.length}`);

let samples = null;
if (samplesDir) {
  samples = {};
  for (const f of fs.readdirSync(samplesDir)) if (/\.wav$/i.test(f)) samples[f] = fs.readFileSync(path.join(samplesDir, f)).toString("base64");
}

const tpl = fs.readFileSync(path.join(__dirname, "template.html"), "utf8");
const html = tpl.replace("__EMX_B64__", emx.toString("base64")).replace("__WAVES__", JSON.stringify(waves))
  .replace("__SAMPLES__", () => JSON.stringify(samples));
const out = path.join(__dirname, samplesDir ? "emx-player-local.html" : "emx-player.html");
fs.writeFileSync(out, html);
console.log(`${path.basename(out)}: ${(html.length / 1024).toFixed(0)} KB` + (samples ? `, ${Object.keys(samples).length} samples` : ""));
