// Compares emx_parse.js output with EMX1Config's labelled log, field by field.
// Usage: node tools/verify_against_log.js factory/FACTORY.EMX factory/FACTORY_log.txt
const fs = require("fs");
const { parseLog } = require("./emx_log");
const { parseFile } = require("./emx_parse");

const [emxFile, logFile] = process.argv.slice(2);
const data = parseFile(fs.readFileSync(emxFile));
const pats = parseLog(fs.readFileSync(logFile, "latin1"));
const stats = {}, bad = {};
const check = (field, got, want, where) => {
  const s = (stats[field] = stats[field] || [0, 0]);
  if (String(got) === String(want)) s[0]++;
  else { s[1]++; (bad[field] = bad[field] || []).length < 3 && bad[field].push(`${where}: ${got} != ${want}`) }
};
const panStr = v => (v === 0 ? "C" : v < 0 ? "L" + -v : "R" + v);
const sgn = v => (v > 0 ? "+" + v : String(v));
const onoff = b => (b ? "on" : "off");

for (const lp of pats) {
  const p = data.patterns[lp.number - 1], w = lp.label;
  check("name", p.name, lp.name.trim(), w);
  check("tempo", p.bpm, parseFloat(lp.header.Tempo), w);
  check("swing", p.swing + "%", lp.header.Swing, w);
  check("length", p.measures, lp.header.Length, w);
  check("lastStep", p.lastStep, lp.header.LastStep, w);
  check("beat", p.beat, lp.header.Beat, w);
  check("rolls", p.rolls, lp.header.Rolls, w);
  check("fxChain", p.fxChain, lp.header["FX chain"], w);
  check("scale", p.arp.scale, lp.header.scale, w);
  p.effects.forEach((fx, i) => {
    const l = lp.header["FX" + (i + 1)];
    check("fx.type", fx.type, l.type, w);
    check("fx.motionSeq", onoff(fx.motionSeq), l.MotionSeq, w);
  });
  for (const part of [...p.drums.map(d => ["D" + d.part, d]), ...p.synths.map(s => ["S" + s.part, s])]) {
    const [name, x] = part, row = lp.parts[name];
    if (!row) continue;
    const at = `${w} ${name}`;
    if (name[0] === "D") { check("d.wave", x.waveName, row.Wave, at); check("d.pitch", x.pitch, +row.Pitch, at) }
    else {
      check("s.osc/wave", x.waveName ? `${x.oscName}/${x.waveName}` : x.oscName, row["Osc/Wave"], at);
      check("s.tune", x.tune, +row.Tune, at); check("s.filter", x.filter, row.Filter, at);
      check("s.cutoff", x.cutoff, row.CutOff, at); check("s.res", x.resonance, row.Res, at);
      check("s.egInt", x.egIntensity, +row.EGIntens, at); check("s.drive", x.drive, row.Drive, at);
      check("s.edit1", x.oscEdit1, row.OSCEdit1, at); check("s.edit2", x.oscEdit2, row.OSCEdit2, at);
      check("s.glide", x.glide, row.Glide, at);
    }
    check("level", x.level, row.Level, at); check("pan", panStr(x.pan), row.Pan, at);
    check("egTime", x.egTime, row.EGTime, at); check("ampEG", x.ampEG, row.AmpEG, at);
    check("fxSel", "FX" + x.fxSelect, row.FXSel, at); check("fxSend", onoff(x.fxSend), row.FXSend, at);
    check("roll", onoff(x.roll), row.Roll, at); check("motion", x.motion.replace("trigHold", "trghld"), row.Motion, at);
    check("modType", x.mod.type, row.ModType, at); check("modDest", x.mod.dest.replace("OscEdit", "OscEd"), row.ModDest.replace("OscEdit", "OscEd"), at);
    check("bpmSync", onoff(x.mod.bpmSync), row.BPMSync, at); check("modDepth", sgn(x.mod.depth), row.ModDepth, at);
    if (name[0] === "S" || !x.mod.bpmSync) check("modSpeed", x.mod.speed, row.ModSpeed, at);
    if (lp.flags["Swing On"]) check("swingOn", x.swing, lp.flags["Swing On"][name], at);
    if (lp.flags["Accent On"]) check("accentOn", x.accent, lp.flags["Accent On"][name], at);
    if (name[0] === "S" && lp.gates[name]) x.notes.forEach(n => { const per = p.beat.endsWith("Tri") ? 12 : 16, s = n.step - 1, cell = Math.floor(s / 16) * per + (s % 16); if (s % 16 < per && s < p.measures * 16) check("gate", n.gate >= 65 ? n.gate : n.gate.toFixed(2), lp.gates[name][cell], at + " step " + n.step) });
  }
}
for (const [k, [ok, ko]] of Object.entries(stats)) console.log(`${k.padEnd(14)} ${ok} ok ${ko ? ko + " FAIL  " + bad[k].join(" | ") : ""}`);
