#pragma once
// XEMX engine: EMX-1 sequencer locked to the host transport, 9 drum + 5 synth parts,
// 3 effect slots with the EMX chain options, then the valve stage and master volume
// (signal path from the service manual block diagram: DSP -> Gain -> Tube -> Master).
#include "EmxData.h"
#include "Fx.h"
#include "SampleBank.h"
#include "Voices.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class Engine
{
public:
    struct Transport { bool hostPlaying = false; double ppq = 0, bpm = 120; };

    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>* in, const juce::MidiBuffer& midi, const Transport& t);

    // Called from the UI thread: plays a part now (pads, keyboard mode, arpeggiator).
    void audition (int part, int note);
    void selectPattern (int index);                 // switches at the end of the pattern while playing
    void playSong (int songIndex);                  // -1 = pattern mode
    void stopAll() { stopRequest = true; }
    PartParams paramsAt (emx::Pattern, int part, int idx) const;

    emx::File file;
    juce::CriticalSection fileLock;                 // held by the UI while replacing the file
    SampleBank samples;

    std::atomic<int> selectedPart { 0 }, currentPattern { 0 }, pendingPattern { -1 }, song { -1 }, songPos { 0 };
    std::atomic<bool> internalPlay { false }, tempoLock { true }, audioThru { false };
    std::atomic<float> masterVolume { 100 }, tubeGain { 40 }, audioInLevel { 100 };  // 0..127 panel knobs
    std::atomic<uint32_t> muteMask { 0 }, soloMask { 0 };
    // display
    std::atomic<int> playStep { -1 }, playMeasure { 0 };
    std::atomic<bool> running { false };
    std::atomic<float> beatPhase { 0 }, outputLevel { 0 };
    std::atomic<double> displayBpm { 120 };
    std::atomic<uint32_t> partFlash { 0 };          // bit per part, set when it triggers

private:
    struct Event { juce::int64 time; int kind; int part; int note; float gain; float gate; PartParams p; };
    enum { kDrumHit, kSynthNote, kSynthMotion, kFxMotion, kSynthOff };

    void scanSteps (emx::Pattern, double ppqStart, double ppqEnd, double samplesPerPpq);
    void doStep (emx::Pattern, int stepIdx, juce::int64 t0, float stepSamples);
    void onPatternEnd();
    void applyEvent (const Event&);
    void renderVoices (int from, int to, const float* audioIn);
    bool audible (int part) const;

    double fs = 44100;
    juce::int64 clock = 0;
    std::vector<Event> events;
    juce::SpinLock auditionLock;
    std::vector<std::pair<int, int>> auditions;
    std::atomic<bool> stopRequest { false };

    DrumVoice drums[emx::kNumDrums][2];
    int drumNext[emx::kNumDrums] {};
    SynthVoice synths[emx::kNumSynths];
    FxSlot fx[3];
    int fxEdit[3][2] {};                             // effect edits after motion sequences
    juce::AudioBuffer<float> dry, fxBus[3], inMono;

    int seqPattern = 0, stepIdx = 0;
    double nextStepPpq = 0, expectedPpq = -1, internalPpq = 0, bar = 88200;
    bool wasRunning = false;
    float valveDc[2] {}, valveLp[2] {}, meter = 0;
    dsp::DcBlock dc[2];
};
