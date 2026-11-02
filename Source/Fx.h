#pragma once
// One EMX effect slot: 16 types, two edit knobs, stereo in place.
// The DSP of the hardware (uPD77213) is not documented; each type follows the panel's description
// with approximate curves. Delay memory matches the 2 Mbit SRAM of the original (about 3 s).
#include "Dsp.h"
#include <juce_audio_basics/juce_audio_basics.h>

class FxSlot
{
public:
    void prepare (double sampleRate);
    void configure (int type, int edit1, int edit2);  // safe to call every block
    void setBpm (double b) { bpm = (float) b; }
    void process (float* L, float* R, int n);
    int type() const { return fxType; }

private:
    void reset();
    float fs = 44100, bpm = 120;
    int fxType = -1, e1 = 0, e2 = 0;
    float s1 = 0, s2 = 0;            // smoothed edit values (0..127)

    dsp::DelayLine dl[2];
    float lfoPh = 0, carrierPh = 0, delaySmooth[2] {}, env = 0;
    juce::Reverb reverb;
    dsp::Svf svf[2][2];
    dsp::Biquad lo[2], hi[2];
    float ap[2][4] {}, apFb[2] {};
    float holdVal[2] {}, holdPh = 0;
    int grainPos = 0, grainLen = 1, grainStart = 0, grainAge = 0;
    float shiftPh = 0;
};
