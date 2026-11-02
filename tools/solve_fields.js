// Finds, for every field EMX1Config labels in its log, the bits of the .EMX file that determine it.
// A candidate (byte offset, bit shift, bit width) "explains" a field when the same bit value never
// maps to two different labelled values across all factory patterns and parts.
// Usage: node tools/solve_fields.js factory/FACTORY.EMX factory/FACTORY_log.txt
const fs = require("fs");
const { parseLog, PART_ORDER } = require("./emx_log");

const [emxFile, logFile] = process.argv.slice(2);
const emx = fs.readFileSync(emxFile);
const pats = parseLog(fs.readFileSync(logFile, "latin1"));
const P_BASE = 0x200, P_SIZE = 0x12c6;
const patBytes = n => emx.subarray(P_BASE + (n - 1) * P_SIZE, P_BASE + n * P_SIZE);
const partBlock = (p, part) => {
  const i = PART_ORDER.indexOf(part);
  return i < 9 ? p.subarray(0x18 + i * 26, 0x18 + i * 26 + 10) : p.subarray(0x102 + (i - 9) * 276, 0x102 + (i - 9) * 276 + 20);
};

function solve(records, nBytes) { // records: [{ bytes, value }]
  const values = new Set(records.map(r => r.value));
  const found = [];
  for (let o = 0; o < nBytes; o++) for (let w = 1; w <= 8; w++) for (let s = 0; s + w <= 8; s++) {
    const map = new Map(); let ok = true;
    for (const r of records) {
      const k = (r.bytes[o] >> s) & ((1 << w) - 1);
      if (map.has(k) && map.get(k) !== r.value) { ok = false; break }
      map.set(k, r.value);
    }
    if (!ok) continue;
    // reverse determinism: each value comes from one bit pattern (excludes fields that are just "wide enough")
    const rev = new Map(); let bij = true;
    for (const [k, v] of map) { if (rev.has(v) && rev.get(v) !== k) bij = false; rev.set(v, k) }
    found.push({ o, s, w, bij, map });
  }
  // prefer bijective, then narrowest
  found.sort((a, b) => (b.bij - a.bij) || (a.w - b.w));
  // drop candidates that are sub-ranges of a better one in the same byte
  return { values: values.size, best: found.slice(0, 3) };
}

function report(title, records, nBytes) {
  if (!records.length) return;
  const { values, best } = solve(records, nBytes);
  const fmt = c => `+${c.o} bits ${c.s}..${c.s + c.w - 1}${c.bij ? "" : " (não bijetivo)"}: ` +
    [...c.map].sort((a, b) => a[0] - b[0]).slice(0, 10).map(([k, v]) => `${k}=${v}`).join(" ") + (c.map.size > 10 ? " …" : "");
  console.log(`${title} [${values} valores]` + (best.length ? "\n   " + best.map(fmt).join("\n   ") : "  -> NÃO ENCONTRADO"));
}

// ---- part parameters ----
for (const kind of ["D", "S"]) {
  const recs = {};
  for (const p of pats) {
    const pb = patBytes(p.number);
    for (const [part, row] of Object.entries(p.parts)) {
      if (part[0] !== kind) continue;
      const bytes = partBlock(pb, part);
      for (const [k, v] of Object.entries(row)) if (k !== "part") (recs[k] = recs[k] || []).push({ bytes, value: v });
    }
  }
  console.log(`\n===== ${kind === "D" ? "DRUM" : "SYNTH"} (offsets relativos ao bloco da parte) =====`);
  for (const [k, r] of Object.entries(recs)) report(k, r, kind === "D" ? 10 : 20);
}

// ---- pattern header + tail (0x08..0x17 and 0x666..0x695) ----
const HDR = [...Array(0x10).keys()].map(i => 0x08 + i).concat([...Array(0x30).keys()].map(i => 0x666 + i));
const hdrBytes = n => { const p = patBytes(n); return HDR.map(o => p[o]) };
console.log("\n===== CABEÇALHO (índice -> offset: 0..15 = 0x08..0x17, 16.. = 0x666..) =====");
const hrecs = {};
for (const p of pats) {
  const bytes = hdrBytes(p.number);
  const add = (k, v) => (hrecs[k] = hrecs[k] || []).push({ bytes, value: v });
  for (const k of ["Swing", "scale", "centre", "Rolls", "FX chain", "Beat"]) add(k, p.header[k]);
  for (const fx of ["FX1", "FX2", "FX3"]) {
    const f = p.header[fx]; add(fx + ".type", f.type); add(fx + ".MotionSeq", f.MotionSeq);
    const ks = Object.keys(f).filter(k => k !== "type" && k !== "MotionSeq");
    ks.forEach((k, i) => add(`${fx}.edit${i + 1}`, f.type + ":" + f[k]));
  }
  for (const [flag, parts] of Object.entries(p.flags)) for (const [part, on] of Object.entries(parts)) add(`${flag} ${part}`, String(on));
}
for (const [k, r] of Object.entries(hrecs)) {
  const { values, best } = solve(r, HDR.length);
  const c = best[0];
  const where = c ? `0x${HDR[c.o].toString(16)} bits ${c.s}..${c.s + c.w - 1}${c.bij ? "" : " (não bij.)"}  ` + [...c.map].sort((a, b) => a[0] - b[0]).slice(0, 8).map(([a, v]) => `${a}=${v}`).join(" ") : "NÃO ENCONTRADO";
  console.log(`${k.padEnd(22)} [${values}] ${where}`);
}
