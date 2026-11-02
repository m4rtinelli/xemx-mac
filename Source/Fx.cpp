#include "Fx.h"
#include "EmxData.h"

using namespace dsp;

void FxSlot::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    for (auto& d : dl) d.init ((int) (fs * 3.1f));
    reverb.setSampleRate (sampleRate);
    reset();
}

void FxSlot::reset()
{
    for (auto& d : dl) d.clear();
    reverb.reset();
    for (auto& c : svf) for (auto& f : c) f.reset();
    lfoPh = carrierPh = env = shiftPh = 0;
    for (auto& d : delaySmooth) d = 0;
    for (auto& c : ap) for (auto& v : c) v = 0;
    apFb[0] = apFb[1] = 0;
    grainAge = grainPos = 0;
}

void FxSlot::configure (int type, int edit1, int edit2)
{
    if (type != fxType) { fxType = type; reset(); s1 = (float) edit1; s2 = (float) edit2; }
    e1 = edit1; e2 = edit2;
}

void FxSlot::process (float* L, float* R, int n)
{
    if (fxType < 0) return;
    // edits glide a little so motion sequences and knob moves don't click
    const float k = 1.0f - std::exp (-(float) n / (0.02f * fs));
    s1 += ((float) e1 - s1) * k; s2 += ((float) e2 - s2) * k;
    const float a = s1 / 127.0f, b = s2 / 127.0f;
    const float bar = 240.0f / std::max (20.0f, bpm) * fs;   // samples per bar
    float* ch[2] { L, R };

    switch (fxType)
    {
        case 0: // Reverb: Time, Level
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.3f + 0.69f * a; p.damping = 0.45f; p.wetLevel = 0.75f * b; p.dryLevel = 1.0f; p.width = 1.0f;
            reverb.setParameters (p);
            reverb.processStereo (L, R, n);
            break;
        }
        case 1: case 2: case 3: // BPM Sync / Short / Mod Delay: Time, Depth (feedback)
        {
            const float target = fxType == 2 ? expMap (s1, 0.004f, 0.75f) * fs : std::min (3.0f * fs, emx::delayNoteBars (e1) * bar);
            const float fb = b * 0.88f, inc = 0.6f / fs;
            for (int i = 0; i < n; ++i)
            {
                float mod[2] { 0, 0 };
                if (fxType == 3) { lfoPh += inc; if (lfoPh >= 1) lfoPh -= 1; mod[0] = std::sin (kTwoPi * lfoPh) * 0.0025f * fs; mod[1] = std::cos (kTwoPi * lfoPh) * 0.0025f * fs; }
                for (int c = 0; c < 2; ++c)
                {
                    delaySmooth[c] += (target - delaySmooth[c]) * 0.0004f;
                    const float d = dl[c].read (delaySmooth[c] + mod[c]);
                    dl[c].push (ch[c][i] + d * fb);
                    ch[c][i] += d * 0.55f;
                }
            }
            break;
        }
        case 4: // Grain Shifter: Speed (grain length), Depth (mix)
        {
            const int idx = std::min (16, 11 + (int) std::round (a * 5));
            const int len = std::max (32, (int) (emx::kSyncNotes[idx] * bar));
            const int size = (int) dl[0].buf.size();
            for (int i = 0; i < n; ++i)
            {
                dl[0].push (L[i]); dl[1].push (R[i]);
                if (grainAge <= 0) { grainLen = len; grainStart = dl[0].w; grainAge = std::max (len * 4, (int) (bar / 4)); grainPos = 0; }
                --grainAge;
                const float win = std::min (1.0f, std::min ((float) grainPos, (float) (grainLen - grainPos)) / 64.0f);
                for (int c = 0; c < 2; ++c)
                {
                    int r = grainStart - grainLen + grainPos; while (r < 0) r += size;
                    const float g = dl[c].buf[(size_t) (r % size)] * win;
                    ch[c][i] = ch[c][i] * (1 - b) + g * b;
                }
                if (++grainPos >= grainLen) grainPos = 0;
            }
            break;
        }
        case 5: // Chorus/Flanger: LFO Rate, Depth
        {
            const float inc = expMap (s1, 0.05f, 12.0f) / fs, depth = (0.0005f + b * 0.0045f) * fs, base = 0.006f * fs;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += inc; if (lfoPh >= 1) lfoPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    const float m = std::sin (kTwoPi * (lfoPh + c * 0.25f));
                    const float d = dl[c].read (base + depth * m);
                    dl[c].push (ch[c][i] + d * 0.35f);
                    ch[c][i] = ch[c][i] * 0.8f + d * 0.7f;
                }
            }
            break;
        }
        case 6: // Phaser: LFO Rate, Depth
        {
            const float inc = expMap (s1, 0.05f, 10.0f) / fs;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += inc; if (lfoPh >= 1) lfoPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    const float f = 900.0f * std::pow (2.0f, (0.2f + b * 2.4f) * std::sin (kTwoPi * (lfoPh + c * 0.25f)));
                    const float t = std::tan (kPi * std::min (f, fs * 0.45f) / fs), coef = (t - 1) / (t + 1);
                    float x = ch[c][i] + apFb[c] * 0.3f;
                    for (auto& z : ap[c]) { const float y = coef * x + z; z = x - coef * y; x = y; }
                    apFb[c] = x;
                    ch[c][i] = (ch[c][i] + x) * 0.7f;
                }
            }
            break;
        }
        case 7: // Ring Mod: Frequency, Balance
        {
            const float inc = expMap (s1, 20.0f, 6000.0f) / fs;
            for (int i = 0; i < n; ++i)
            {
                carrierPh += inc; if (carrierPh >= 1) carrierPh -= 1;
                const float m = std::sin (kTwoPi * carrierPh);
                for (int c = 0; c < 2; ++c) ch[c][i] = ch[c][i] * (1 - b) + ch[c][i] * m * b;
            }
            break;
        }
        case 8: // Talking Mod: Formant (vowel), Offset
        {
            const float off = std::pow (2.0f, (float) emx::signed64 ((int) s2) / 63.0f);
            for (int c = 0; c < 2; ++c) { svf[c][0].set ((300 + 500 * a) * off, 6, fs); svf[c][1].set ((2300 - 1400 * a) * off, 8, fs); }
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float l, bp1, bp2, h;
                    svf[c][0].process (ch[c][i], l, bp1, h); svf[c][1].process (ch[c][i], l, bp2, h);
                    ch[c][i] = (bp1 + bp2 * 0.7f) * 2.2f;
                }
            break;
        }
        case 9: // Pitch Shifter: Pitch (semitones), Balance. Two crossfaded delay taps.
        {
            const float ratio = std::pow (2.0f, std::round ((s1 - 64) * 0.3f) / 12.0f);
            const float win = 0.06f * fs, rate = (1.0f - ratio) / win;
            for (int i = 0; i < n; ++i)
            {
                shiftPh += rate; if (shiftPh >= 1) shiftPh -= 1; if (shiftPh < 0) shiftPh += 1;
                const float p2 = std::fmod (shiftPh + 0.5f, 1.0f);
                const float g1 = std::sin (kPi * shiftPh), g2 = std::sin (kPi * p2);
                for (int c = 0; c < 2; ++c)
                {
                    dl[c].push (ch[c][i]);
                    const float y = dl[c].read (2 + shiftPh * win) * g1 + dl[c].read (2 + p2 * win) * g2;
                    ch[c][i] = ch[c][i] * (1 - b) + y * b;
                }
            }
            break;
        }
        case 10: // Compressor: Sensitivity, Attack
        {
            const float thr = std::pow (10.0f, -a * 45.0f / 20.0f), att = std::exp (-1.0f / (expMap (s2, 0.001f, 0.25f) * fs));
            const float rel = std::exp (-1.0f / (0.12f * fs)), makeup = 1.0f + a * 1.5f;
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (L[i]), std::abs (R[i]));
                env = x > env ? att * env + (1 - att) * x : rel * env + (1 - rel) * x;
                const float g = env > thr ? std::pow (env / thr, 1.0f / 8.0f - 1.0f) : 1.0f;
                L[i] *= g * makeup; R[i] *= g * makeup;
            }
            break;
        }
        case 11: // Distortion: Gain, Level
        {
            const float g = 1.0f + a * 60.0f, norm = 1.0f / std::tanh (g);
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) ch[c][i] = std::tanh (g * ch[c][i]) * norm * b * 0.9f;
            break;
        }
        case 12: // Decimator: Sampling Freq (sample & hold), Sampling Bit
        {
            const float inc = expMap (s1, 400.0f, fs) / fs;
            const float steps = std::pow (2.0f, std::round (2 + b * 14) - 1);
            for (int i = 0; i < n; ++i)
            {
                holdPh += inc;
                if (holdPh >= 1) { holdPh -= 1; for (int c = 0; c < 2; ++c) holdVal[c] = std::round (ch[c][i] * steps) / steps; }
                for (int c = 0; c < 2; ++c) ch[c][i] = holdVal[c];
            }
            break;
        }
        case 13: // EQ: Low gain 80 Hz, High gain 12 kHz
        {
            for (int c = 0; c < 2; ++c)
            {
                lo[c].shelf (false, 80, (float) emx::signed64 ((int) s1) / 63.0f * 15.0f, fs);
                hi[c].shelf (true, 12000, (float) emx::signed64 ((int) s2) / 63.0f * 15.0f, fs);
            }
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) ch[c][i] = hi[c].process (lo[c].process (ch[c][i]));
            break;
        }
        case 14: case 15: // LPF / HPF: Cutoff, Resonance
        {
            for (int c = 0; c < 2; ++c) svf[c][0].set (expMap (s1, 30, 18000), 0.7f + b * 16, fs);
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float l, bp, h; svf[c][0].process (ch[c][i], l, bp, h);
                    ch[c][i] = fxType == 14 ? l : h;
                }
            break;
        }
        default: break;
    }
}
