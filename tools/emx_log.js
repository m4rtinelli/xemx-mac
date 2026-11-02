// Parses the text log EMX1Config writes for View > Pattern > "Ditto + note grids".
// Returns one record per pattern with every labelled value as a string, exactly as printed.
const fs = require("fs");

const PART_ORDER = ["D1", "D2", "D3", "D4", "D5", "D6A", "D6B", "D7A", "D7B", "S1", "S2", "S3", "S4", "S5"];

function patternNumber(label) { // "A.01" -> 1, "B.01" -> 65
  const [bank, n] = label.split(".");
  return (bank.charCodeAt(0) - 65) * 64 + parseInt(n, 10);
}

function parseTableRow(line, cols) {
  const part = line.slice(0, 4).trim();
  const wave = line.slice(4, 20).trim();
  const vals = line.slice(20).trim().split(/\s+/);
  const row = { part, [cols[0]]: wave };
  cols.slice(1).forEach((c, i) => (row[c] = vals[i]));
  return row;
}

function parseFlagRow(line) { // "Swing    On: D 1 2 . 4 ... S 1 2 . 4 5" -> { D1: true, D3: false, ... }
  const toks = line.split(":")[1].trim().split(/\s+/);
  const out = {}; let side = "", k = 0;
  const names = { D: PART_ORDER.slice(0, 9), S: PART_ORDER.slice(9) };
  for (const t of toks) {
    if (t === "D" || t === "S") { side = t; k = 0; continue }
    out[names[side][k++]] = t !== ".";
  }
  return out;
}

function parseLog(text) {
  const lines = text.split(/\r?\n/);
  const pats = [];
  let cur = null, cols = null, section = null;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    let m;
    if ((m = line.match(/^Pattern ([A-D]\.\d\d) (.*?), file/))) {
      cur = { label: m[1], number: patternNumber(m[1]), name: m[2], header: {}, flags: {}, parts: {}, motion: [], gates: {} };
      pats.push(cur); section = null; continue;
    }
    if (!cur) continue;
    if (/^(Tempo|Swing|Rolls):/.test(line)) {
      for (const kv of line.split(/,\s*/)) { const [k, ...v] = kv.split(":"); if (v.length) cur.header[k.trim()] = v.join(":").trim() }
      if ((m = line.match(/centre (\S+)/))) cur.header.centre = m[1];
      if ((m = line.match(/Arpeg scale: ([^,]+)/))) cur.header.scale = m[1].trim();
      continue;
    }
    if ((m = line.match(/^(FX[123]): (.*)$/))) {
      const parts = m[2].split(/,\s*/);
      const fx = { type: parts[0] };
      for (const p of parts.slice(1)) { const mm = p.match(/^(.*\S)\s+(\S+)$/); if (mm) fx[mm[1]] = mm[2] }
      cur.header[m[1]] = fx; continue;
    }
    if ((m = line.match(/^\s*(Swing\s+On|Accent\s+On|Output\s+L\/R|3\/4):/))) {
      cur.flags[m[1].replace(/\s+/g, " ")] = parseFlagRow(line); continue;
    }
    if (/^\s+(Wave|Osc\/Wave)\s/.test(line)) { cols = line.trim().split(/\s+/); continue }
    if (cols && /^(D[1-7][AB]?|S[1-5])\s/.test(line) && !line.includes("|")) {
      const row = parseTableRow(line, cols); cur.parts[row.part] = row; continue;
    }
    if (/^Motion seqs/.test(line)) { section = "motion"; cols = null; continue }
    if (/^Gate times/.test(line)) { section = "gate"; continue }
    if (section && (m = line.match(/^(D[1-7][AB]?|S[1-5])\s+(.*?)\s*\|(.*)$/))) {
      const cells = m[3].split("|").join(" ").trim().split(/\s+/);
      if (section === "motion") cur.motion.push({ part: m[1], param: m[2].trim(), values: cells });
      else cur.gates[m[1]] = cells;
    }
  }
  return pats;
}

module.exports = { parseLog, PART_ORDER };

if (require.main === module) {
  const pats = parseLog(fs.readFileSync(process.argv[2], "latin1"));
  console.log(JSON.stringify(process.argv[3] ? pats.find(p => p.number == process.argv[3]) : pats.length, null, 1));
}
