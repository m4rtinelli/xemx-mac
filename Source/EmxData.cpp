#include "EmxData.h"
#include "EmxTables.inc"
#include <BinaryData.h>

namespace emx
{
const char* const kOscTypes[16] = { "Waveform", "Dual Osc", "Chord", "Unison", "Ring Mod", "Osc Sync", "Cross Mod", "VPM",
                                    "WS", "Additive", "Comb", "Formant", "Noise", "PCM+Comb", "PCM+WS", "AudioIn+Comb" };
const char* const kFxTypes[16] = { "Reverb", "BPM Sync Delay", "Short Delay", "Mod Delay", "Grain Shifter", "Cho/Flg", "Phaser", "Ring Mod",
                                   "Talking Mod", "Pitch Shifter", "Compressor", "Distortion", "Decimator", "EQ", "LPF", "HPF" };
const char* const kPartNames[kNumParts] = { "1", "2", "3", "4", "5", "6A", "6B", "7A", "7B", "S1", "S2", "S3", "S4", "S5" };
const float kSyncNotes[17] = { 8, 4, 2, 1, 0.75f, 0.5f, 0.375f, 1.0f / 3, 0.25f, 0.1875f, 1.0f / 6, 0.125f, 1.0f / 12, 0.0625f, 1.0f / 24, 1.0f / 32, 1.0f / 64 };
const char* const kSyncNames[17] = { "8/1", "4/1", "2/1", "1/1", "3/4", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32", "1/64" };

juce::StringArray waveNames (int osc)
{
    const juce::StringArray basic { "Saw", "Squ", "Tri", "Sin" }, wave4 { "Saw", "Pulse", "Tri", "Sin" };
    juce::StringArray pairs, unison;
    for (auto& a : basic) for (auto b : { "Saw", "Squ", "Tri", "Sin", "Ns" }) pairs.add (a + juce::String (b));
    for (auto& w : basic) for (int n = 3; n <= 6; ++n) unison.add (juce::String (n) + w);
    switch (osc)
    {
        case 0: case 5: return wave4;
        case 1: case 4: case 6: return pairs;
        case 2: case 7: case 9: return basic;
        case 3: return unison;
        case 8: return { "Type1", "Type2" };
        case 10: { auto c = basic; c.add ("Noise"); return c; }
        case 13: case 14: { juce::StringArray p; for (auto* n : kPcmWaves) p.add (n); return p; }
        default: return {};
    }
}

// Breakpoints read off EMX1Config's display of the factory patterns.
static const std::pair<int, int> kDelaySteps[] { { 0, 16 }, { 10, 15 }, { 17, 14 }, { 28, 13 }, { 36, 12 }, { 45, 11 }, { 55, 10 },
                                                 { 64, 9 }, { 74, 8 }, { 88, 6 }, { 102, 5 }, { 112, 4 }, { 121, 3 } };
static int delayIndex (int b) { int idx = 16; for (auto& [from, i] : kDelaySteps) if (b >= from) idx = i; return idx; }
float delayNoteBars (int b) { return kSyncNotes[delayIndex (b)]; }
juce::String delayNoteName (int b) { return kSyncNames[delayIndex (b)]; }

juce::String Pattern::name() const
{
    juce::String s;
    for (int i = 0; i < 8; ++i) s << juce::juce_wchar (p[i] >= 32 && p[i] < 127 ? p[i] : ' ');
    return s.trimEnd();
}

void Pattern::setName (const juce::String& n)
{
    for (int i = 0; i < 8; ++i) p[i] = uint8_t (i < n.length() && n[i] >= 32 && n[i] < 127 ? n[i] : ' ');
}

void Pattern::setBpm (double bpm)
{
    bpm = juce::jlimit (20.0, 300.0, bpm);
    const int whole = int (bpm), tenths = juce::jlimit (0, 9, int (std::round ((bpm - whole) * 10)));
    p[8] = uint8_t (whole >> 1);
    p[9] = uint8_t ((p[9] & 0x70) | ((whole & 1) << 7) | tenths);
}

void Pattern::setMask (int off, int part, bool on)
{
    int m = (p[off] << 8) | p[off + 1];
    m = on ? (m | (1 << part)) : (m & ~(1 << part));
    p[off] = uint8_t (m >> 8); p[off + 1] = uint8_t (m);
}

void Pattern::toggleStep (int part, int s, int defaultNote)
{
    if (! isSynth (part)) { setBit (drum (part) + 10, s, ! drumStep (part, s)); return; }
    auto* sy = synth (part - kNumDrums);
    if (sy[20 + s] & 0x80) { sy[20 + s] = uint8_t (defaultNote & 0x7f); if (sy[148 + s] == 0) sy[148 + s] = 2; }
    else sy[20 + s] |= 0x80;
}

bool Pattern::empty() const
{
    for (int d = 0; d < kNumDrums; ++d) for (int b = 0; b < 16; ++b) if (drum (d)[10 + b]) return false;
    for (int s = 0; s < kNumSynths; ++s) for (int k = 0; k < kSteps; ++k) if (! (synth (s)[20 + k] & 0x80)) return false;
    return true;
}

int motionSlots (const Pattern& pat, MotionSlot (&out)[24])
{
    int count = 0;
    for (int m = 0; m < 24; ++m)
    {
        const uint8_t* o = pat.p + 0x696 + m * 130;
        const int id = (o[0] << 8) | o[1];
        if (id == 0xffff) continue;
        const int part = id >> 5, prm = id & 31;
        MotionSlot s;
        s.param = prm; s.values = o + 2;
        s.part = part < 5 ? kNumDrums + part : part < 14 ? part - 5 : part == 15 ? 14 : -1;
        if (s.part >= 0) out[count++] = s;
    }
    return count;
}

File::File() { loadFactory(); }

void File::loadFactory()
{
    data.replaceAll (BinaryData::FACTORY_EMX, (size_t) BinaryData::FACTORY_EMXSize);
}

bool File::load (const void* src, size_t size, juce::String& error)
{
    auto* b = static_cast<const uint8_t*> (src);
    if (size < 12 || memcmp (b, "KORG", 4) != 0 || memcmp (b + 8, "EMX", 3) != 0)
    { error = "Not an EMX-1 file (KORG/EMX header missing). ESX-1 files are not supported."; return false; }
    if (size < (size_t) (kSongBase + kNumSongs * kSongSize)) { error = "File too short for an EMX-1 card image."; return false; }
    data.replaceAll (src, juce::jmin (size, (size_t) kFileSize));
    if (data.getSize() < (size_t) kFileSize) data.setSize (kFileSize, true);
    return true;
}

std::vector<Song> File::songs() const
{
    std::vector<Song> out;
    auto* b = static_cast<const uint8_t*> (data.getData());
    for (int i = 0; i < kNumSongs; ++i)
    {
        const uint8_t* o = b + kSongBase + i * kSongSize;
        Song s;
        for (int k = 0; k < 8; ++k) s.name << juce::juce_wchar (o[k] >= 32 && o[k] < 127 ? o[k] : ' ');
        s.name = s.name.trimEnd();
        s.bpm = o[8] * 2 + (o[9] >> 7) + (o[9] & 0x0f) / 10.0;
        for (int k = 0; k <= o[0x0b]; ++k) s.patterns.push_back (o[0x10 + k]);
        out.push_back (std::move (s));
    }
    return out;
}
} // namespace emx
