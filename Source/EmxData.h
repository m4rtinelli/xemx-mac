#pragma once
// EMX-1 .emx file model. All edits go straight into the raw bytes, so the file can be saved back
// unchanged apart from what was edited. Layout: see EMX_FORMAT.md (verified against EMX1Config).
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <vector>

namespace emx
{
constexpr int kFileSize = 0x1A8000, kPatBase = 0x200, kPatSize = 0x12C6, kNumPatterns = 256;
constexpr int kSongBase = 0x130000, kSongSize = 0x210, kNumSongs = 64;
constexpr int kNumDrums = 9, kNumSynths = 5, kNumParts = 14, kSteps = 128;
constexpr int kDrumSize = 26, kSynthSize = 276, kDrumBase = 0x18, kSynthBase = 0x102;

extern const char* const kDrumWaves[207];
extern const char* const kPcmWaves[76];
extern const char* const kOscTypes[16];
extern const char* const kFxTypes[16];
extern const char* const kScales[31];
extern const char* const kPartNames[kNumParts]; // "1".."7B", "S1".."S5"
extern const float kSyncNotes[17];              // bars: 8/1 .. 1/64
extern const char* const kSyncNames[17];

juce::StringArray waveNames (int oscType);       // wave list of a synth oscillator type (empty if none)
float delayNoteBars (int timeByte);              // FX delay Time byte -> note length in bars
juce::String delayNoteName (int timeByte);

inline int signed64 (int v) { return juce::jmax (-63, v - 64); }
inline float gateSteps (int g) { return g < 128 ? (g + 1) * 0.25f : g < 192 ? 16.5f + (g - 128) * 0.5f : float (g - 127); }
inline float stepPpq (int beat) { static const float t[] { 0.25f, 0.125f, 1.0f / 3, 1.0f / 6 }; return t[beat & 3]; }

// One pattern, viewed in place inside the file bytes.
struct Pattern
{
    uint8_t* p = nullptr;

    juce::String name() const;
    void setName (const juce::String&);
    double bpm() const { return p[8] * 2 + (p[9] >> 7) + (p[9] & 0x0f) / 10.0; }
    void setBpm (double);
    int swing() const { return 50 + (p[0x0a] & 0x1f); }           // percent
    int measures() const { return (p[0x0b] & 7) + 1; }
    int beat() const { return (p[0x0b] >> 4) & 3; }
    int rolls() const { return ((p[0x0b] >> 6) & 3) + 2; }
    int fxChain() const { return p[0x0c] & 3; }
    int lastStep() const { return juce::jmin (beat() >= 2 ? 12 : 16, p[0x0d] + 1); }
    int scale() const { return juce::jmin (30, p[0x0e] & 0x1f); }
    int arpCentre() const { return p[0x0f]; }
    bool mask (int off, int part) const { return (((p[off] << 8) | p[off + 1]) >> part) & 1; }
    void setMask (int off, int part, bool on);
    bool swingOn (int part) const { return mask (0x12, part); }
    bool outputLR (int part) const { return mask (0x14, part); }
    bool accentOn (int part) const { return mask (0x16, part); }

    uint8_t* drum (int i) const { return p + kDrumBase + i * kDrumSize; }
    uint8_t* synth (int i) const { return p + kSynthBase + i * kSynthSize; }
    // part 0..8 drums, 9..13 synths
    uint8_t* part (int i) const { return i < kNumDrums ? drum (i) : synth (i - kNumDrums); }
    static bool isSynth (int part) { return part >= kNumDrums; }

    static bool bit (const uint8_t* bm, int s) { return (bm[s >> 3] >> (s & 7)) & 1; }
    static void setBit (uint8_t* bm, int s, bool on) { if (on) bm[s >> 3] |= uint8_t (1 << (s & 7)); else bm[s >> 3] &= uint8_t (~(1 << (s & 7))); }
    bool drumStep (int d, int s) const { return bit (drum (d) + 10, s); }
    int note (int sy, int s) const { auto n = synth (sy)[20 + s]; return (n & 0x80) ? -1 : (n & 0x7f); }
    int gateByte (int sy, int s) const { return synth (sy)[148 + s]; }
    bool stepOn (int part, int s) const { return isSynth (part) ? note (part - kNumDrums, s) >= 0 : drumStep (part, s); }
    void toggleStep (int part, int s, int defaultNote);
    bool accentStep (bool synthSide, int s) const { return bit (p + (synthSide ? 0x67a : 0x668), s); }
    void toggleAccent (bool synthSide, int s) { auto* bm = p + (synthSide ? 0x67a : 0x668); setBit (bm, s, ! bit (bm, s)); }
    int accentLevel (bool synthSide) const { return p[synthSide ? 0x678 : 0x666]; }

    uint8_t* fx (int k) const { return p + 0x68a + k * 4; }  // type, edit1, edit2, motion bit0
    bool empty() const;
};

// Common bits of a part block: flags byte (drum +5, synth +14) and mod byte (drum +6, synth +16).
struct PartOffsets { int pitchOrGlide, level, pan, egTime, flags, mod, speed, depth, motion; };
inline PartOffsets offsets (bool synth)
{
    return synth ? PartOffsets { 5, 11, 12, 13, 14, 16, 17, 18, 19 } : PartOffsets { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
}

struct MotionSlot { int part = -1; int param = -1; const uint8_t* values = nullptr; }; // part: 0..13 (drums first), 14 = FX
int motionSlots (const Pattern&, MotionSlot (&out)[24]); // returns count, no allocation

struct Song
{
    juce::String name;
    double bpm = 120;
    std::vector<int> patterns; // 0-based
};

class File
{
public:
    File();
    bool load (const void* data, size_t size, juce::String& error);
    void loadFactory();
    const juce::MemoryBlock& bytes() const { return data; }
    Pattern pattern (int i) { return { static_cast<uint8_t*> (data.getData()) + kPatBase + i * kPatSize }; }
    std::vector<Song> songs() const;

private:
    juce::MemoryBlock data;
};
} // namespace emx
