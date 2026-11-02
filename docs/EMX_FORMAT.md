# Formato do arquivo .EMX (Korg Electribe EMX-1)

Engenharia reversa a partir do arquivo de fábrica embutido no EMX1Config v3.01
(offset 792344 do `.exe`, extraído em `factory/FACTORY.EMX`), conferida campo a campo
contra o log rotulado que o próprio EMX1Config gera dos 224 patterns de fábrica
(`node tools/verify_against_log.js factory/FACTORY.EMX factory/FACTORY_log.txt`: 0 divergências
em ~95 mil valores). Nomes de waves, osciladores, escalas e efeitos vêm das tabelas de strings do `.exe`.

Status: ✅ confirmado pelo log · 🟡 provável · ❓ localizado, não decodificado.

## Arquivo (sempre 0x1A8000 = 1.736.704 bytes)

| Offset | Tam. | Conteúdo | |
|---|---|---|---|
| 0x000000 | 0x200 | Cabeçalho `KORG 00 00 00 69 "EMX" 00` + global | ✅ / ❓ |
| 0x000200 | 256 × 0x12C6 | Patterns 1–256 | ✅ |
| 0x12C800 | 0x3800 | ❓ (dominado por 0xC3) | ❓ |
| 0x130000 | 64 × 0x210 | Songs 1–64 | ✅ |
| 0x138400 | … | Pool de eventos de song (compartilhado) | ❓ |
| 0x160000 | 0x48000 | ❓ (dominado por 0xCC) | ❓ |

## Pattern (0x12C6 bytes)

| Offset | Campo | |
|---|---|---|
| 0x00 | Nome, 8 bytes ASCII | ✅ |
| 0x08–09 | BPM = `b8*2 + (b9>>7)` + décimos `(b9 & 0x0F)/10` | ✅ |
| 0x0A | Swing % = 50 + bits 0–4 | ✅ |
| 0x0B | bits 0–2 compassos−1 · bits 4–5 beat (16th, 32nd, 8Tri, 16Tri) · bits 6–7 rolls (2, 3, 4) | ✅ |
| 0x0C | bits 0–1 FX chain (off, 1>2, 2>3, 1>2>3) | ✅ |
| 0x0D | Último step − 1 | ✅ |
| 0x0E | bits 0–4 escala do arpejador (tabela abaixo) · bits 5–7 ❓ | ✅ / ❓ |
| 0x0F | Nota central do arpejador (MIDI) | ✅ |
| 0x10–11 | ❓ | ❓ |
| 0x12–13 | Máscara Swing On (14 bits) | ✅ |
| 0x14–15 | Máscara Output L/R (constante na fábrica) | 🟡 |
| 0x16–17 | Máscara Accent On | ✅ |
| 0x18 | 9 partes de bateria × 26 bytes: 1, 2, 3, 4, 5, 6A, 6B, 7A, 7B | ✅ |
| 0x102 | 5 partes de synth × 276 bytes | ✅ |
| 0x666 | Accent da bateria: nível, 00, bitmap de 16 bytes em 0x668 | ✅ (nível 🟡) |
| 0x678 | Accent do synth: nível, 00, bitmap em 0x67A | ✅ (nível 🟡) |
| 0x68A | 3 efeitos × 4 bytes: tipo, edit1, edit2, bit 0 = motion seq | ✅ |
| 0x696 | 24 motion sequences × 130 bytes | ✅ |

Máscaras de 14 bits (BE): bit 0–8 = D1…D7B, bit 9–13 = S1…S5.
Bitmaps de step: 16 bytes, LSB primeiro (bit n do byte k = step k·8+n). Em 8Tri/16Tri cada compasso
usa só os 12 primeiros dos 16 slots.

### Parâmetros compartilhados (drum +5…+9, synth +14, +16…+19)

| Byte | Campo |
|---|---|
| flags | bits 0–1 FX Select (FX1–3) · bit 2 FX Send · bit 3 Roll · bit 4 Amp EG (1 = EG, 0 = gate) |
| mod | bits 0–2 Mod Dest (Pitch, OscEd1, OscEd2, Cutoff, Amp, Pan) · bits 4–6 Mod Type (saw, squ, tri, S&H, EG) · bit 7 BPM Sync |
| speed | Mod Speed; com BPM Sync: índice `round(b·16/127)` em 8/1, 4/1, 2/1, 1/1, 3/4, 1/2, 3/8, 1/3, 1/4, 3/16, 1/6, 1/8, 1/12, 1/16, 1/24, 1/32, 1/64 |
| depth | Mod Depth = b − 64 (mín. −63) |
| motion | Motion: 0 off, 1 smooth, 2 trig hold |

### Parte de bateria (26 bytes)

| +0 wave (0–206) | +1 pitch = b−64 | +2 level | +3 pan = b−64 (0 = L64, 64 = C) | +4 EG time | +5…+9 compartilhados | +10 bitmap de 16 bytes |
|---|---|---|---|---|---|---|

### Parte de synth (276 bytes)

| Offset | Campo |
|---|---|
| +0 | Wave (índice dentro da lista do tipo de oscilador) |
| +1 | Tune = b − 50 (semitons) |
| +2 | Tipo de oscilador: Wave, Dual, Chord, Unison, RingMod, Sync, XMod, VPM, WS, Additive, Comb, Formant, Noise, PCM+Cmb, PCM+WS, AudioIn+Comb |
| +3, +4 | Osc Edit 1, Osc Edit 2 |
| +5 | Glide |
| +6 | Filtro: LP, HP, BP, BP+ |
| +7 … +13 | Cutoff, Resonance, EG Int (b−64), Drive, Level, Pan (b−64), EG Time |
| +14, +16…+19 | Compartilhados |
| +15 | ❓ |
| +20 | 128 notas: bit 7 = sem nota, bits 0–6 = nota MIDI |
| +148 | 128 gates, em steps: `g<128 → (g+1)/4`, `g<192 → 16,5 + (g−128)/2`, senão `g−127` (255 = 128 = tie) |

Listas de wave por oscilador: Wave/Sync = Saw, Pulse, Tri, Sin · Chord/VPM/Additive = Saw, Squ, Tri, Sin ·
Comb = + Noise · Unison = 3–6 × Saw/Squ/Tri/Sin (índice = forma·4 + vozes−3) · Dual/RingMod/XMod = pares
`a·5 + b` com a ∈ {Saw, Squ, Tri, Sin}, b ∈ {Saw, Squ, Tri, Sin, Ns} · WS = Type1, Type2 · PCM = 76 waves
(lista em `emx_parse.js`).

### Efeitos

Tipos: Reverb, BPM Sync Delay, Short Delay, Mod Delay, Grain Shifter, Cho/Flg, Phaser, Ring Mod, Talking Mod,
Pitch Shifter, Compressor, Distortion, Decimator, EQ, LPF, HPF. Edits são o byte cru, exceto EQ e Talking
Offset (b−64), Pitch Shifter (≈ `round((b−64)·0,3)` semitons) e Delay Time das versões sincronizadas
(faixas → 1/64, 1/32, 1/24, 1/16, 1/12, 1/8, 1/6, 3/16, 1/4, 3/8, 1/2, 3/4).

### Motion sequences

Cada slot: id de 16 bits BE (`FF FF` = vazio) + 128 valores (0xFF/0x80 = sem dado). `parte = id >> 5`
(0–4 = S1–S5, 5–13 = D1…D7B, 15 = FX), `parâmetro = id & 31`.
Bateria: 1 Pitch, 7 Level, 8 Pan, 9 EGTime, 10 AmpEG, 11 Roll, 13 FXSend, 14 FXSelect, 16 ModDepth.
Synth: 3 OscEdit1, 4 OscEdit2, 5 Glide, 6 FilterType, 7 Cutoff, 8 Res, 9 EGInt, 10 Drive, 11 Level, 12 Pan,
13 EGTime, 15 Roll, 17 FXSend, 18 FXSelect, 19 ModType, 20 ModDepth, 21 ModSpeed, 22 ModDest.
FX (🟡): 5/6, 9/10, 13/14 = edit1/edit2 de FX1, FX2, FX3. Valores de chaves usam a escala 0–127 do knob.

### Escalas (0x0E bits 0–4)

Chromatic, Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Major Blues, minor Blues, Diminish,
Combination Diminish, Major Pentatonic, minor Pentatonic, Raga Bhairav, Raga Gamanasrama, Raga Todi, Spanish,
Gypsy, Arabian, Egyptian, Hawaiian, Bali Island Pelog, Japanese Miyakobushi, Ryukyu, Wholetone, m3rd, M3rd,
4th, 5th, Octave interval.

## Song (0x210 bytes)

| Offset | Campo | |
|---|---|---|
| 0x00 | Nome | ✅ |
| 0x08 | BPM (mesma codificação do pattern) | ✅ |
| 0x0B | Posições − 1 | ✅ |
| 0x0D | Próxima song | 🟡 |
| 0x0E | Nº de eventos (u16 BE) no pool 0x138400 | 🟡 |
| 0x10 | Lista de patterns (base 0) | ✅ |

## Ainda em aberto

Pattern 0x10–11, synth +15, bits altos de 0x0E, escala exata do nível de accent, formato do pool de eventos
de song, regiões 0x12C800 e 0x160000. Nada disso aparece no log do EMX1Config, então nenhum afeta o som.
O que falta para soar idêntico são as curvas de cada knob (medidas no hardware) e os 76 PCMs do synth.
