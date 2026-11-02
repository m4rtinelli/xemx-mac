#pragma once
// Drum and synth voices. Each EMX part is monophonic; a new hit or note cuts the previous one.
#include "Dsp.h"
#include "SampleBank.h"
#include <memory>

// Parameters of one part at one step, raw 0..127 knob scale, after motion sequences are applied.
struct PartParams
{
    int wave = 0, osc = 0, tune = 0, pitch = 64, glide = 0, level = 100, pan = 64, egTime = 64;
    int ampEG = 0, roll = 0, fxSend = 0, fxSel = 0, modType = 0, modDest = 0, bpmSync = 0, modSpeed = 0, modDepth = 64, motion = 0;
    int edit1 = 0, edit2 = 0, filter = 0, cutoff = 127, reso = 0, egInt = 64, drive = 0;
};

// Shared per-part LFO (saw, squ, tri, S&H, EG), retriggered on each note like the hardware's EG type.
struct PartLfo
{
    float ph = 0, held = 0, egLevel = 0;
    dsp::Noise rng;
    void trigger() { ph = 0; egLevel = 1; held = rng.next(); }
    float next (int type, float hz, float fs);
};

class DrumVoice
{
public:
    void prepare (float sampleRate) { fs = sampleRate; }
    void trigger (const PartParams& p, float gain, std::shared_ptr<const DrumSample> s, int waveIndex, float barSamples);
    void render (float* L, float* R, int n);   // adds into L/R
    bool active() const { return playing; }
    void release() { fadeMul = std::exp (-1.0f / (0.003f * fs)); }
    PartParams params;

private:
    float synthDrum();
    float fs = 44100, gain = 0, pos = 0, rate = 1, amp = 1, decayMul = 1, fade = 1, fadeMul = 1, panL = 0.7f, panR = 0.7f;
    float bar = 88200, t = 0, f0 = 100;
    int category = 0;
    bool playing = false;
    std::shared_ptr<const DrumSample> sample;
    PartLfo lfo;
    dsp::Noise noise;
    dsp::Svf tone;
    dsp::Osc o1;
};

class SynthVoice
{
public:
    void prepare (float sampleRate);
    void noteOn (const PartParams& p, int note, float gateSamples, float gain, float barSamples);
    void noteOff() { gateLeft = 0; }
    int lastNote() const { return noteNum; }
    void update (const PartParams& p) { const bool w = p.wave != params.wave || p.osc != params.osc; params = p; if (w) setPcm(); } // motion while held
    struct Pcm { int shape = 0; float detune = 0; bool pair = false, pluck = false; };
    void render (float* L, float* R, int n, const float* audioIn);
    bool active() const { return playing; }
    PartParams params;

private:
    float oscSample (float freq, const float* audioIn, int i);
    void setPcm();
    Pcm pcm;
    bool attacking = false;
    int noteNum = -1;
    float fs = 44100, gain = 0, freq = 440, target = 440, glideCoef = 0, ampEnv = 0, filtEnv = 0, gateLeft = 0, bar = 88200;
    float pluck = 1;
    bool playing = false, released = false;
    dsp::Osc osc[8];
    float phases[8] {}, syncPh = 0, fmPh = 0;
    dsp::Svf filter, form[2];
    dsp::DelayLine comb;
    float combLp = 0;
    dsp::Noise noise;
    PartLfo lfo;
    std::vector<float> table;  // additive wavetable
    int tableKey = -1;
    int ctl = 0;
    float cutHz = 1000;
};
