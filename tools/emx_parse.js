// Parser for Korg Electribe EMX-1 SmartMedia/SD files (.EMX, 0x1A8000 bytes).
// Layout reverse-engineered from the factory file embedded in EMX1Config v3.01 and checked
// field by field against EMX1Config's own labelled log of all 224 factory patterns
// (tools/verify_against_log.js). See EMX_FORMAT.md.
//
// Usage: node emx_parse.js FILE.EMX [patternNumber]   -> JSON on stdout

const fs = require("fs");

const FILE_SIZE = 0x1a8000;
const PATTERN_BASE = 0x200, PATTERN_SIZE = 0x12c6, PATTERN_COUNT = 256;
const SONG_BASE = 0x130000, SONG_SIZE = 0x210, SONG_COUNT = 64;
const DRUM_NAMES = ["1", "2", "3", "4", "5", "6A", "6B", "7A", "7B"];
const STEPS = 128; // 8 measures x 16 steps

// ---- value tables (order taken from EMX1Config's string tables) ----
const DRUM_WAVES = ["BD-Dark","BD-99 1","BD-99 2","BD-Syn1","BD-Syn2","BD-Syn3","BD-Syn4","BD-Syn5","BD-Syn6","BD-Syn7","BD-Syn8","BD-Syn9","BD-Syn10","BD-Dist1","BD-Dist2","BD-Dist3","BD-Dist4","BD-Dist5","BD-Dist6","BD-Dist7","BD-Squas","BD-88 1","BD-88 2","BD-Digi","BD-DDD1","BD-DDD2","BD-Lynn","BD-Dry1","BD-Dry2","BD-Dry3","BD-Dry4","BD-Soft","BD-Hip","BD-R&B","BD-Jazz","BD-Break","BD-Ambi","BD-Def","BD-D&B1","BD-D&B2","BigBreak","SD-99 1","SD-99 2","SD-99 3","SD-99 4","SD-99 5","SD-88 1","SD-88 2","SD-88 3","SD-Syn1","SD-Syn2","SD-77","SD-Lynn","SD-Disco","SD-Dry1","SD-Dry2","SD-Dry3","SD-Dry4","SD-Dry5","SD-Dry6","SD-Ambi1","SD-Ambi2","SD-Ambi3","SD-Picl1","SD-Picl2","SD-Picl3","SD-Bras1","SD-Bras2","SD-Crckl","SD-Brk1","SD-Brk2","SD-Brk3","SD-Brk4","SD-D&B","SD-Clap1","SD-Clap2","SD-R&B1","SD-R&B2","SD-R&B3","SD-R&B4","SD-R&B5","RM-Ambi1","RM-Ambi2","RM-Dry","RM-DDD","RM-Lynn","RM-88","Clp-99 1","Clp-99 2","Clp-88 1","Clp-88 2","Clp-Nois","Clp-R&B1","Clp-R&B2","HH-99 1C","HH-99 1O","HH-99 2C","HH-99 2H","HH-99 2O","HH-99 3C","HH-99 3O","HH-88 C","HH-88 O","HH-Syn1C","HH-Syn1H","HH-Syn1O","HH-Syn2C","HH-Syn2O","HH-Syn3C","HH-Syn3O","HH-Nrm1C","HH-Nrm1O","HH-Nrm2C","HH-Nrm2O","HH-CrspC","HH-CrspO","HH-OldC","HH-OldO","HH-LynnC","HH-LynnO","Rid-99 1","Rid-99 2","Rid-KPR","Rid-Edg1","Rid-Edg2","Rid-Jazz","Crs-99 1","Crs-99 2","Crs-Norm","Crs-Spls","Tom-99","Tom-88","Tom-Simm","Tom-NrmH","Tom-NrmL","Tom-NrmF","Tom-Jazz","Bng-Hi","Bng-Slap","Bng-Lo1","Bng-Lo2","Cng-Hi1","Cng-Hi2","Cng-HiMt","Cng-Lo1","Cng-Lo2","Cng-LoMt","Cng-LynH","Cng-LynL","Timb-Hi1","Timb-Hi2","Timb-Lo1","Timb-Lo2","Timb-Rim","Claves","Cowbell","ChaChaBl","MamboBel","Agogo","Triangle","Tambouri","Junk1","Junk2","SleighBl","Shaker1","Shaker2","Cabasa1","Cabasa2","Cabasa3","Guiro-S","Guiro-L","Wbl-DDDH","Wbl-DDDL","Whistle","Baya-Ghe","Baya-Mt1","Baya-Mt2","Tbla-Na","Tbla-Tin","Tbla-Mt1","Tbla-Mt2","Djmb-1a","Djmb-1b","Djmb-1c","Djmb-2a","Djmb-2b","Djmb-2c","Udu","Taiko-Op","Taiko-Rm","Tsuzumi","GtrWah","Zap1","Zap2","SynPerc1","SynPerc2","SynPerc3","SynPerc4","SynPerc5","SynPerc6","Rev-BD","Rev-SD1","Rev-SD2","Rev-Crsh","Scratch1","Scratch2","Scratch3"];
const PCM_WAVES = ["Piano","E.Piano","Clav","M1-Organ","Organ","Marimba","Vibe","Cymbell","Flute","AltoSax","M1-T.Sax","Trumpet","MuteTp","BrassEns","VoiceAh","M1-Choir","VoiceWav","Violin","Strings","Pizzicat","F.Guitar","A.Guitar","MuteGtr","FunkGtr","Sitar","A.Bass","E.Bass","M1-Bass","PickBass","SlapBass","FMBass","88Bass","BoostSaw","SawSqMix","HPFSaw","OctBass1","OctBass2","Saw5th","Squ5th","SynSin1","SynSin2","SynSin3","SynSin4","SynSin5","SynWire1","SynWire2","Digi1","Digi2","Digi3","Digi4","SynVox1","SynVox2","Endless","Syn-FX1","Syn-FX2","OrchHit","BandHit1","BandHit2","DiscoHit","RaveHit1","RaveHit2","RaveHit3","RaveHit4","RaveHit5","RaveHit6","CH-Piano","CH-M1Pia","CH-EPian","CH-Organ","CH-Strgs","CH-Gtr1","CH-Gtr2","DR-BDs","DR-SDs","DR-CymTm","DR-Percs"];
const PAIRS = ["SawSaw","SawSqu","SawTri","SawSin","SawNs","SquSaw","SquSqu","SquTri","SquSin","SquNs","TriSaw","TriSqu","TriTri","TriSin","TriNs","SinSaw","SinSqu","SinTri","SinSin","SinNs"];
const BASIC = ["Saw", "Squ", "Tri", "Sin"];
const OSC_TYPES = [ // [name, wave list for byte +0]
  ["Wave", ["Saw", "Pulse", "Tri", "Sin"]], ["Dual", PAIRS], ["Chord", BASIC],
  ["Unison", ["3Saw","4Saw","5Saw","6Saw","3Squ","4Squ","5Squ","6Squ","3Tri","4Tri","5Tri","6Tri","3Sin","4Sin","5Sin","6Sin"]],
  ["RingMod", PAIRS], ["Sync", ["Saw", "Pulse", "Tri", "Sin"]], ["XMod", PAIRS], ["VPM", BASIC], ["WS", ["Type1", "Type2"]],
  ["Additive", BASIC], ["Comb", [...BASIC, "Noise"]], ["Formant", null], ["Noise", null],
  ["PCM+Cmb", PCM_WAVES], ["PCM+WS", PCM_WAVES], ["AudioIn+Comb", null],
];
const FILTERS = ["LP", "HP", "BP", "BP+"];
const MOD_TYPES = ["saw", "squ", "tri", "S&H", "EG"];
const MOD_DESTS = ["Pitch", "OscEdit1", "OscEdit2", "Cutoff", "Amp", "Pan"]; // drums use Pitch/Amp/Pan
const MOTION_MODES = ["off", "smooth", "trigHold"];
const SYNC_NOTES = ["8/1","4/1","2/1","1/1","3/4","1/2","3/8","1/3","1/4","3/16","1/6","1/8","1/12","1/16","1/24","1/32","1/64"];
const BEATS = ["16th", "32nd", "8Tri", "16Tri"];
const FX_TYPES = [ // [name, edit1 label, edit2 label]
  ["Reverb","Time","Level"],["BPM Sync Delay","Time","Depth"],["Short Delay","Time","Depth"],["Mod Delay","Time","Depth"],
  ["Grain Shifter","Speed","Depth"],["Cho/Flg","LFO Rate","Depth"],["Phaser","LFO Rate","Depth"],["Ring Mod","Frequency","Balance"],
  ["Talking Mod","Formant","Offset"],["Pitch Shifter","Pitch","Balance"],["Compressor","Sensitivity","Attack"],["Distortion","Gain","Level"],
  ["Decimator","Sampling Freq","Sampling Bit"],["EQ","Low Gain (80Hz)","High Gain (12kHz)"],["LPF","Cutoff Freq","Resonance"],["HPF","Cutoff Freq","Resonance"],
];
const SCALES = ["Chromatic","Ionian","Dorian","Phrygian","Lydian","Mixolydian","Aeolian","Locrian","Major Blues","minor Blues","Diminish","Combination Diminish","Major Pentatonic","minor Pentatonic","Raga Bhairav","Raga Gamanasrama","Raga Todi","Spanish","Gypsy","Arabian","Egyptian","Hawaiian","Bali Island Pelog","Japanese Miyakobushi","Ryukyu","Wholetone","m3rd interval","M3rd interval","4th interval","5th interval","Octave interval"];
// Motion sequence target: part = id >> 5 (0-4 synth 1-5, 5-13 drum 1..7B, 15 effects), param = id & 31.
const MOTION_PARTS = ["S1","S2","S3","S4","S5","D1","D2","D3","D4","D5","D6A","D6B","D7A","D7B",null,"FX"];
const MOTION_PARAMS_DRUM = { 1: "Pitch", 7: "Level", 8: "Pan", 9: "EGTime", 10: "AmpEG", 11: "Roll", 13: "FXSend", 14: "FXSelect", 16: "ModDepth" };
const MOTION_PARAMS_SYNTH = { 3: "OscEdit1", 4: "OscEdit2", 5: "Glide", 6: "FilterType", 7: "Cutoff", 8: "Resonance", 9: "FilterEGInt", 10: "Drive", 11: "Level", 12: "Pan", 13: "EGTime", 15: "Roll", 17: "FXSend", 18: "FXSelect", 19: "ModType", 20: "ModDepth", 21: "ModSpeed", 22: "ModDest" };
const MOTION_PARAMS_FX = { 5: "FX1 Edit1", 6: "FX1 Edit2", 9: "FX2 Edit1", 10: "FX2 Edit2", 13: "FX3 Edit1", 14: "FX3 Edit2" }; // probable

const name = (b, o) => b.toString("latin1", o, o + 8).replace(/\s+$/, "");
// Tempo: 9-bit integer BPM across two bytes, tenths in the low nibble of the second.
const tempo = (b, o) => b[o] * 2 + (b[o + 1] >> 7) + (b[o + 1] & 0x0f) / 10;
const signed = v => Math.max(-63, v - 64);          // bipolar knobs: 0x40 = 0, 0/1 both show -63
const pan = v => v - 64;                            // -64 = L64, 0 = centre, +63 = R63
// Gate in steps: quarter-step up to 127, half-step to 191, whole steps above (255 = 128 = tie through pattern).
const gateSteps = g => (g < 128 ? (g + 1) * 0.25 : g < 192 ? 16.5 + (g - 128) * 0.5 : g - 127);
const mask14 = (b, o) => (b[o] << 8) | b[o + 1]; // bit 0..8 = D1..D7B, bit 9..13 = S1..S5

// 16-byte step bitmap, LSB first: bit n of byte k = step k*8+n.
function bitmap(b, o) {
  const steps = [];
  for (let s = 0; s < STEPS; s++) if (b[o + (s >> 3)] & (1 << (s & 7))) steps.push(s + 1);
  return steps;
}

// Bytes shared by drum (+5..+9) and synth (+14, +16..+19) blocks.
function common(p, flags, mod, speed, depth, motion) {
  const f = p[flags], m = p[mod], bpmSync = !!(m & 0x80);
  return {
    ampEG: f & 0x10 ? "EG" : "gate", fxSelect: (f & 3) + 1, fxSend: !!(f & 4), roll: !!(f & 8),
    mod: { type: MOD_TYPES[(m >> 4) & 7], dest: MOD_DESTS[m & 7], bpmSync,
      speed: bpmSync ? SYNC_NOTES[Math.round(p[speed] * 16 / 127)] : p[speed], depth: signed(p[depth]) },
    motion: MOTION_MODES[p[motion] & 3],
  };
}

function parseDrum(p, i, swingMask, accentMask) {
  const o = 0x18 + i * 26;
  return {
    part: DRUM_NAMES[i],
    wave: p[o], waveName: DRUM_WAVES[p[o]],
    pitch: signed(p[o + 1]), level: p[o + 2], pan: pan(p[o + 3]), egTime: p[o + 4],
    ...common(p, o + 5, o + 6, o + 7, o + 8, o + 9),
    swing: !!(swingMask >> i & 1), accent: !!(accentMask >> i & 1),
    steps: bitmap(p, o + 10),
  };
}

function parseSynth(p, i, swingMask, accentMask) {
  const o = 0x102 + i * 276;
  const [oscName, waves] = OSC_TYPES[p[o + 2]] || ["?", null];
  const notes = [];
  for (let s = 0; s < STEPS; s++) {
    const n = p[o + 20 + s];
    if (!(n & 0x80)) notes.push({ step: s + 1, note: n & 0x7f, gate: gateSteps(p[o + 148 + s]) });
  }
  return {
    part: String(i + 1),
    osc: p[o + 2], oscName, wave: p[o], waveName: waves ? waves[p[o]] : null,
    tune: p[o + 1] - 50, oscEdit1: p[o + 3], oscEdit2: p[o + 4], glide: p[o + 5],
    filter: FILTERS[p[o + 6] & 3], cutoff: p[o + 7], resonance: p[o + 8], egIntensity: signed(p[o + 9]), drive: p[o + 10],
    level: p[o + 11], pan: pan(p[o + 12]), egTime: p[o + 13],
    ...common(p, o + 14, o + 16, o + 17, o + 18, o + 19),
    swing: !!(swingMask >> (9 + i) & 1), accent: !!(accentMask >> (9 + i) & 1),
    notes, // MIDI note numbers; bit 7 set in file = step off; gate in steps
  };
}

function parseMotion(p) {
  const out = [];
  for (let m = 0; m < 24; m++) {
    const o = 0x696 + m * 130;
    if (p[o] === 0xff && p[o + 1] === 0xff) continue;
    const id = (p[o] << 8) | p[o + 1], part = MOTION_PARTS[id >> 5], prm = id & 31;
    const table = part === "FX" ? MOTION_PARAMS_FX : part && part[0] === "D" ? MOTION_PARAMS_DRUM : MOTION_PARAMS_SYNTH;
    // 0xFF / 0x80 = no value recorded on that step (the knob keeps its last value)
    out.push({ slot: m + 1, part, param: table[prm] || `#${prm}`, values: [...p.subarray(o + 2, o + 130)].map(v => (v === 0xff || v === 0x80 ? null : v)) });
  }
  return out;
}

function parseEffect(p, i) {
  const o = 0x68a + i * 4;
  const [type, l1, l2] = FX_TYPES[p[o] & 15];
  return { slot: i + 1, type, [l1]: p[o + 1], [l2]: p[o + 2], motionSeq: !!(p[o + 3] & 1) };
}

function parsePattern(b, idx) {
  const p = b.subarray(PATTERN_BASE + idx * PATTERN_SIZE, PATTERN_BASE + (idx + 1) * PATTERN_SIZE);
  const swingMask = mask14(p, 0x12), accentMask = mask14(p, 0x16);
  return {
    number: idx + 1,
    name: name(p, 0),
    bpm: tempo(p, 8),
    swing: 50 + p[0x0a],                          // percent
    measures: (p[0x0b] & 0x07) + 1,
    beat: BEATS[(p[0x0b] >> 4) & 3],
    rolls: ((p[0x0b] >> 6) & 3) + 2,              // roll subdivisions
    fxChain: ["off", "1>2", "2>3", "1>2>3"][p[0x0c] & 3],
    lastStep: p[0x0d] + 1,
    arp: { scale: SCALES[p[0x0e] & 0x1f], arpFlags: p[0x0e] >> 5, centre: p[0x0f] }, // centre as MIDI note
    outputMask: mask14(p, 0x14),                  // probable: parts routed to L/R (all on in factory data)
    drums: DRUM_NAMES.map((_, i) => parseDrum(p, i, swingMask, accentMask)),
    synths: [0, 1, 2, 3, 4].map(i => parseSynth(p, i, swingMask, accentMask)),
    accents: { drum: { level: p[0x666], steps: bitmap(p, 0x668) }, synth: { level: p[0x678], steps: bitmap(p, 0x67a) } },
    effects: [0, 1, 2].map(i => parseEffect(p, i)),
    motion: parseMotion(p),
  };
}

function parseSong(b, idx) {
  const o = SONG_BASE + idx * SONG_SIZE;
  const positions = b[o + 0x0b] + 1;
  return {
    number: idx + 1,
    name: name(b, o),
    bpm: tempo(b, o + 8),
    nextSong: b[o + 0x0d],
    eventCount: b.readUInt16BE(o + 0x0e),
    patterns: [...b.subarray(o + 0x10, o + 0x10 + positions)].map(x => x + 1),
  };
}

function parseFile(b) {
  if (b.toString("latin1", 0, 4) !== "KORG" || b.toString("latin1", 8, 11) !== "EMX")
    throw new Error("not an EMX-1 file");
  if (b.length !== FILE_SIZE) console.error(`warning: size ${b.length}, expected ${FILE_SIZE}`);
  const patterns = [], songs = [];
  for (let i = 0; i < PATTERN_COUNT; i++) patterns.push(parsePattern(b, i));
  for (let i = 0; i < SONG_COUNT; i++) {
    const s = parseSong(b, i);
    if (s.name) songs.push(s);
  }
  return { patterns, songs };
}

if (require.main === module) {
  const [file, n] = process.argv.slice(2);
  const data = parseFile(fs.readFileSync(file));
  const out = n ? data.patterns[n - 1] : data;
  console.log(JSON.stringify(out, (k, v) => (k === "values" || k === "steps" ? v.join(",") : v), 2));
}

module.exports = { parseFile, parsePattern, parseSong, DRUM_WAVES, PCM_WAVES, OSC_TYPES, FX_TYPES, SCALES, SYNC_NOTES };
