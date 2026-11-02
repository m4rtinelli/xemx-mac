#include "Voices.h"
#include "EmxData.h"

using namespace dsp;

namespace
{
float lfoHz (const PartParams& p, float bar, float fs)
{
    if (p.bpmSync) return fs / std::max (1.0f, emx::kSyncNotes[std::clamp ((int) std::round (p.modSpeed * 16 / 127.0f), 0, 16)] * bar);
    return expMap ((float) p.modSpeed, 0.05f, 30.0f);
}
void panGains (float pan, float& l, float& r)
{
    const float x = std::clamp (pan, -1.0f, 1.0f) * 0.5f + 0.5f;
    l = std::cos (x * kPi * 0.5f); r = std::sin (x * kPi * 0.5f);
}
int drumCategory (const juce::String& n)
{
    auto u = n.toUpperCase();
    if (u.startsWith ("BD") || u == "BIGBREAK") return 0;
    if (u.startsWith ("SD")) return 1;
    if (u.startsWith ("CLP") || u.startsWith ("RM")) return 2;
    if (u.startsWith ("HH")) return u.endsWith ("O") ? 4 : 3;
    if (u.startsWith ("RID") || u.startsWith ("CRS") || u.startsWith ("REV")) return 5;
    return 6;
}
using PcmRecipe = SynthVoice::Pcm;
PcmRecipe pcmRecipe (const juce::String& n)
{
    auto has = [&] (std::initializer_list<const char*> ks) { for (auto* k : ks) if (n.contains (k)) return true; return false; };
    if (has ({ "Bass", "Saw", "HPF", "Boost" })) return { 0, n.contains ("Oct") ? 1200.0f : n.contains ("5th") ? 702.0f : 0.0f };
    if (has ({ "Squ", "Clav", "Digi", "Wire" })) return { 1 };
    if (has ({ "Organ" })) return { 4 };
    if (has ({ "Strings", "Strgs", "Violin", "Brass", "Trumpet", "Sax", "Endless", "Choir", "Voice", "Vox", "Hit" })) return { 0, 9, true };
    if (has ({ "DR-" })) return { 5 };
    PcmRecipe r { has ({ "Sin", "Flute", "Marimba", "Vibe", "Cymbell" }) ? 3 : 2 };
    r.pluck = has ({ "Piano", "Pia", "Guitar", "Gtr", "Pizz", "Sitar", "Marimba", "Vibe", "Cymbell", "EPian" });
    return r;
}
const int kChords[10][4] { { 0, 7, -1, -1 }, { 0, 5, 7, -1 }, { 0, 3, 7, -1 }, { 0, 4, 7, -1 }, { 0, 3, 7, 10 },
                           { 0, 4, 7, 11 }, { 0, 4, 7, 10 }, { 0, 2, 7, -1 }, { 0, 4, 8, -1 }, { 0, 3, 6, -1 } };
const float kFormants[5][2] { { 730, 1090 }, { 270, 2290 }, { 300, 870 }, { 570, 840 }, { 440, 1020 } };
} // namespace

float PartLfo::next (int type, float hz, float fs)
{
    const float inc = hz / fs;
    ph += inc;
    if (ph >= 1) { ph -= 1; held = rng.next(); }
    switch (type)
    {
        case 0: return 2 * ph - 1;
        case 1: return ph < 0.5f ? 1.0f : -1.0f;
        case 2: return 1 - 4 * std::abs (ph - 0.5f);
        case 3: return held;
        default: egLevel *= std::exp (-6.9f * inc); return egLevel;   // EG: one decay per LFO period
    }
}

// ---------------------------------------------------------------- drums
void DrumVoice::trigger (const PartParams& p, float g, std::shared_ptr<const DrumSample> s, int waveIndex, float barSamples)
{
    params = p; sample = std::move (s); bar = barSamples;
    const float semis = (float) emx::signed64 (p.pitch) / 63.0f * 12.0f;
    rate = std::pow (2.0f, semis / 12.0f) * (sample ? (float) sample->rate / fs : 1.0f);
    gain = std::pow (p.level / 127.0f, 1.6f) * 0.95f * g;
    decayMul = p.ampEG ? std::exp (std::log (0.001f) / (egSeconds (p.egTime) * fs)) : 1.0f;
    amp = 1; pos = 0; t = 0; fade = 1; fadeMul = 1; playing = true;
    category = drumCategory (emx::kDrumWaves[std::clamp (waveIndex, 0, 206)]);
    f0 = std::pow (2.0f, semis / 12.0f);
    lfo.trigger();
    tone.reset();
    if (! sample) tone.set (category == 6 ? 900.0f * f0 : category == 1 ? 1500.0f * f0 : 7000.0f, 0.8f, fs);
}

float DrumVoice::synthDrum()
{
    // stand-ins when the sample bank is not loaded
    const float dec = params.ampEG ? 1.0f : 1.6f;
    float y = 0, lp, bp, hp;
    switch (category)
    {
        case 0: { const float f = (42 + 110 * std::exp (-t / 0.03f)) * f0; y = o1.next (3, f / fs) * std::exp (-t / (0.25f * dec)); break; }
        case 1: { y = o1.next (2, 185 * f0 / fs) * std::exp (-t / 0.05f) * 0.6f; tone.process (noise.next(), lp, bp, hp); y += hp * std::exp (-t / (0.09f * dec)) * 0.7f; break; }
        case 2: { const float env = t < 0.03f ? std::exp (-std::fmod (t, 0.01f) / 0.003f) : std::exp (-(t - 0.03f) / 0.12f); y = noise.next() * env * 0.6f; break; }
        case 3: case 4: case 5:
        {
            tone.process (noise.next(), lp, bp, hp);
            const float len = category == 3 ? 0.04f : category == 4 ? 0.3f : 0.9f;
            y = hp * std::exp (-t / (len * dec)) * 0.5f; break;
        }
        default: { y = o1.next (3, 220 * f0 / fs) * std::exp (-t / 0.12f) * 0.7f; tone.process (noise.next(), lp, bp, hp); y += bp * std::exp (-t / 0.01f) * 0.2f; break; }
    }
    t += 1.0f / fs;
    if (t > 3.0f) playing = false;
    return y;
}

void DrumVoice::render (float* L, float* R, int n)
{
    if (! playing) return;
    const float depth = (float) emx::signed64 (params.modDepth) / 63.0f, hz = lfoHz (params, bar, fs);
    const float basePan = (params.pan - 64) / 64.0f;
    float pl, pr; panGains (basePan, pl, pr);
    const float* data = sample ? sample->audio.getReadPointer (0) : nullptr;
    const int len = sample ? sample->audio.getNumSamples() : 0;
    for (int i = 0; i < n && playing; ++i)
    {
        const float m = depth != 0 ? lfo.next (params.modType, hz, fs) * depth : 0.0f;
        float y;
        if (data)
        {
            const int k = (int) pos;
            if (k + 1 >= len) { playing = false; break; }
            const float f = pos - (float) k;
            y = data[k] + (data[k + 1] - data[k]) * f;
            pos += params.modDest == 0 && m != 0 ? rate * std::pow (2.0f, m) : rate;
        }
        else y = synthDrum();
        float g = gain * amp * fade;
        if (params.modDest == 4) g *= 1.0f - std::abs (depth) * 0.5f + m * 0.5f;
        if (params.modDest == 5 && (i & 15) == 0) panGains (basePan + m, pl, pr);
        L[i] += y * g * pl; R[i] += y * g * pr;
        amp *= decayMul; fade *= fadeMul;
        if (amp < 0.0005f || fade < 0.0005f) playing = false;
    }
}

// ---------------------------------------------------------------- synth
void SynthVoice::prepare (float sampleRate)
{
    fs = sampleRate;
    comb.init ((int) (fs / 15));
    table.assign (2048, 0.0f);
}

void SynthVoice::noteOn (const PartParams& p, int note, float gateSamples, float g, float barSamples)
{
    params = p; bar = barSamples; noteNum = note;
    target = mtof ((float) (note + p.tune));
    const bool legato = playing && ! released;
    if (! (p.glide > 0 && playing)) freq = target;
    glideCoef = p.glide > 0 ? std::exp (-1.0f / (expMap ((float) p.glide, 0.005f, 1.2f) * 0.25f * fs)) : 0.0f;
    gain = std::pow (p.level / 127.0f, 1.5f) * 0.42f * g;
    gateLeft = std::max (8.0f, gateSamples);
    filtEnv = 1;
    if (! legato) attacking = true;
    setPcm();
    playing = true; released = false; pluck = 1;
    lfo.trigger();
    if (! legato) { combLp = 0; syncPh = 0; }
    if (p.osc == 9)
    {
        const int harm = 2 + (int) std::round (p.edit1 / 127.0f * 30), key = (p.wave & 3) * 64 + harm;
        if (key != tableKey)
        {
            tableKey = key;
            for (size_t i = 0; i < table.size(); ++i)
            {
                const float x = (float) i / (float) table.size();
                float v = 0;
                for (int h = 1; h <= harm; ++h)
                {
                    const int w = p.wave & 3;
                    const float a = w == 0 ? 1.0f / h : w == 1 ? (h % 2 ? 1.0f / h : 0) : w == 2 ? (h % 2 ? ((h / 2) % 2 ? -1.0f : 1.0f) / (h * h) : 0) : (h == 1 ? 1.0f : 0);
                    v += a * std::sin (kTwoPi * h * x);
                }
                table[i] = v * 0.6f;
            }
        }
    }
}

void SynthVoice::setPcm()
{
    if (params.osc != 13 && params.osc != 14) return;
    auto r = pcmRecipe (emx::kPcmWaves[std::clamp (params.wave, 0, 75)]);
    pcm = { r.shape, r.detune, r.pair, r.pluck };
}

float SynthVoice::oscSample (float f, const float* in, int i)
{
    const PartParams& p = params;
    const float e1 = std::clamp (p.edit1 / 127.0f, 0.0f, 1.0f), e2 = std::clamp (p.edit2 / 127.0f, 0.0f, 1.0f), inc = f / fs;
    auto pairA = [&] { return (p.wave / 5) & 3; };
    auto pairB = [&] { return p.wave % 5; };
    switch (p.osc)
    {
        case 0: // Waveform: Edit1 = pulse width / shape
        {
            const int w = p.wave & 3;
            if (w == 1) return osc[0].next (1, inc, 0.5f - e1 * 0.45f);
            const float y = osc[0].next (w == 0 ? 0 : w, inc);
            return e1 > 0.01f ? std::sin (y * (1 + e1 * 3) * kPi * 0.5f) : y;
        }
        case 1: // Dual: Edit1 osc 2 pitch (0..+24 st), Edit2 osc 2 level
        {
            const float a = osc[0].next (pairA(), inc);
            const float b = pairB() == 4 ? noise.next() : osc[1].next (pairB(), inc * std::pow (2.0f, e1 * 2.0f));
            return 0.6f * a + 0.6f * e2 * b;
        }
        case 2: // Chord: Edit1 chord type, Edit2 voicing
        {
            const int* c = kChords[std::min (9, (int) (e1 * 10))];
            float y = 0;
            for (int k = 0; k < 4 && c[k] >= 0; ++k) y += osc[k].next (p.wave & 3, inc * std::pow (2.0f, (c[k] + (k && e2 > 0.5f ? 12 : 0)) / 12.0f));
            return y * 0.4f;
        }
        case 3: // Unison: 3..6 voices, Edit1 detune
        {
            const int nv = 3 + (p.wave & 3), w = (p.wave >> 2) & 3;
            float y = 0;
            for (int k = 0; k < nv; ++k) y += osc[k].next (w, inc * std::pow (2.0f, ((float) k / (nv - 1) - 0.5f) * e1 * 60.0f / 1200.0f));
            return y * 1.3f / (float) nv;
        }
        case 4: // Ring Mod: Edit1 osc 2 pitch (-24..+24 st)
        {
            const float a = osc[0].next (pairA(), inc);
            const float b = pairB() == 4 ? noise.next() : osc[1].next (pairB(), inc * std::pow (2.0f, (e1 * 48 - 24) / 12.0f));
            return a * b;
        }
        case 5: // Osc Sync: Edit1 slave pitch
        {
            syncPh += inc;
            if (syncPh >= 1) { syncPh -= 1; osc[0].ph = 0; }
            return osc[0].next (p.wave & 3, inc * (1 + e1 * 7));
        }
        case 6: case 7: // Cross Mod (FM) / VPM (phase mod): Edit1 depth, Edit2 ratio
        {
            const bool vpm = p.osc == 7;
            const float ratio = vpm ? 1.0f + std::floor (e2 * 8) : std::pow (2.0f, (e2 * 24 - 12) / 12.0f);
            const float m = (! vpm && pairB() == 4) ? noise.next() : osc[1].next (vpm ? 3 : pairB(), inc * ratio);
            if (vpm)
            {
                phases[0] += inc; if (phases[0] >= 1) phases[0] -= 1;
                const float ph = phases[0] + e1 * 0.5f * m;
                const int w = p.wave & 3;
                const float x = ph - std::floor (ph);
                return w == 3 ? std::sin (kTwoPi * x) : w == 0 ? 2 * x - 1 : w == 1 ? (x < 0.5f ? 1.0f : -1.0f) : 1 - 4 * std::abs (x - 0.5f);
            }
            return osc[0].next (pairA(), inc * (1 + m * e1 * 4));
        }
        case 8: // Waveshape: Edit1 drive
        {
            const float s = osc[0].next (p.wave ? 3 : 2, inc) * (1 + e1 * 6);
            return p.wave ? std::sin (s * kPi * 0.75f) : std::tanh (s * 2);
        }
        case 9: // Additive: Edit1 number of harmonics
        {
            phases[0] += inc; if (phases[0] >= 1) phases[0] -= 1;
            const float x = phases[0] * (float) table.size();
            const int k = (int) x; const float fr = x - (float) k;
            return table[(size_t) k] + (table[(size_t) ((k + 1) % (int) table.size())] - table[(size_t) k]) * fr;
        }
        case 10: case 13: case 15: // Comb (on a wave, a PCM stand-in or the audio input)
        {
            float src;
            if (p.osc == 10) src = p.wave == 4 ? noise.next() : osc[0].next (p.wave & 3, inc);
            else if (p.osc == 15) src = in ? in[i] : 0.0f;
            else
            {
                const auto& r = pcm;
                src = r.shape == 5 ? noise.next() : osc[0].next (r.shape == 4 ? 3 : r.shape, inc);
                if (p.osc == 13 && e1 < 0.05f) return src;
            }
            const float d = comb.read (fs / std::max (20.0f, f));
            combLp += (d - combLp) * (1.0f - e2 * 0.8f);
            comb.push (src * 0.5f + combLp * (0.5f + e1 * 0.47f));
            return (src * 0.3f + combLp) * 0.8f;
        }
        case 11: // Formant: Edit1 vowel, Edit2 shift
        {
            const float s = osc[0].next (0, inc);
            float l, b1, b2, h;
            form[0].process (s, l, b1, h); form[1].process (s, l, b2, h);
            return (b1 + b2 * 0.7f) * 1.6f;
        }
        case 12: // Noise through a band-pass at the note: Edit1 resonance
        {
            float l, b, h; form[0].process (noise.next(), l, b, h); return b * 2.0f;
        }
        case 14: // PCM+WS stand-in
        {
            const auto& r = pcm;
            float y;
            if (r.shape == 5) y = noise.next();
            else if (r.shape == 4) y = (osc[0].next (3, inc) + 0.8f * osc[1].next (3, inc * 2) + 0.6f * osc[2].next (3, inc * 4)) * 0.45f;
            else
            {
                y = osc[0].next (r.shape, inc);
                if (r.detune != 0) y = 0.6f * y + 0.6f * osc[1].next (r.shape, inc * std::pow (2.0f, r.detune / 1200.0f));
                if (r.pair) y += 0.5f * osc[2].next (r.shape, inc * std::pow (2.0f, -r.detune / 1200.0f));
            }
            if (r.pluck) y *= pluck;
            return e1 > 0.02f ? std::tanh (y * (1 + e1 * 6)) : y;
        }
        default: return 0;
    }
}

void SynthVoice::render (float* L, float* R, int n, const float* in)
{
    if (! playing) return;
    const PartParams& p = params;
    const float depth = (float) emx::signed64 (p.modDepth) / 63.0f, hz = lfoHz (p, bar, fs);
    const float egT = egSeconds (p.egTime), ampDecay = std::exp (std::log (0.001f) / (egT * fs)), filtDecay = std::exp (std::log (0.01f) / (egT * fs));
    const float rel = std::exp (-1.0f / (0.008f * fs)), att = 1.0f / (0.003f * fs);
    const float dk = 1.0f + p.drive / 127.0f * 12.0f, dnorm = 1.0f / std::tanh (dk);
    const float pluckMul = std::exp (-1.0f / (0.6f * fs));
    float pl = 0.7f, pr = 0.7f, lfoVal = 0, pitchMul = 1;
    for (int i = 0; i < n && playing; ++i)
    {
        if ((ctl++ & 15) == 0)
        {
            lfoVal = depth != 0 ? lfo.next (p.modType, hz, fs / 16.0f) * depth : 0.0f;
            pitchMul = p.modDest == 0 ? std::pow (2.0f, lfoVal) : 1.0f;
            float oct = (float) emx::signed64 (p.egInt) / 63.0f * 5.0f * filtEnv + (p.modDest == 3 ? lfoVal * 3.0f : 0.0f);
            cutHz = expMap ((float) p.cutoff, 30, 18000) * std::pow (2.0f, oct);
            filter.set (cutHz, 0.7f + std::pow (p.reso / 127.0f, 2.0f) * 22.0f, fs);
            if (p.osc == 11)
            {
                const float e1 = p.edit1 / 127.0f * 4.0f, shift = 0.7f + p.edit2 / 127.0f * 0.6f;
                const int v = std::min (3, (int) e1); const float fr = e1 - (float) v;
                for (int k = 0; k < 2; ++k) form[k].set ((kFormants[v][k] + (kFormants[v + 1][k] - kFormants[v][k]) * fr) * shift, 8, fs);
            }
            if (p.osc == 12) form[0].set (freq, 1.0f + p.edit1 / 127.0f * 30.0f, fs);
            panGains ((p.pan - 64) / 64.0f + (p.modDest == 5 ? lfoVal : 0.0f), pl, pr);
        }
        if (glideCoef > 0) freq = target + (freq - target) * glideCoef;
        float x = oscSample (freq * pitchMul, in, i);
        x = std::tanh (x * dk) * dnorm;
        float lp, bp, hp;
        filter.process (x, lp, bp, hp);
        float y = p.filter == 0 ? lp : p.filter == 1 ? hp : p.filter == 2 ? bp : bp + x * 0.5f;

        // amp: EG = decay set by EG Time, gate = sustain until the gate ends
        if (! released)
        {
            if (attacking) { ampEnv += att; if (ampEnv >= 1) { ampEnv = 1; attacking = false; } }
            else if (p.ampEG) ampEnv *= ampDecay;
            if (--gateLeft <= 0) released = true;
        }
        else { ampEnv *= rel; if (ampEnv < 0.0002f) playing = false; }
        filtEnv *= filtDecay;
        pluck *= pluckMul;
        float g = gain * ampEnv;
        if (p.modDest == 4) g *= 1.0f - std::abs (depth) * 0.5f + lfoVal * 0.5f;
        L[i] += y * g * pl; R[i] += y * g * pr;
    }
}
