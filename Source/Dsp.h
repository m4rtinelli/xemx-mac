#pragma once
// Small DSP building blocks shared by voices and effects.
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace dsp
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float expMap (float v, float lo, float hi) { return lo * std::pow (hi / lo, v / 127.0f); }
inline float egSeconds (int v) { return expMap ((float) v, 0.012f, 4.0f); }   // EG Time knob, approximate curve
inline float mtof (float n) { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }
inline float softClip (float x) { return std::tanh (x); }

struct Noise
{
    uint32_t s = 0x12345678u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

// Topology-preserving state variable filter (Simper). Set coefficients at control rate.
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (float fc, float q, float fs)
    {
        fc = std::clamp (fc, 10.0f, fs * 0.45f);
        const float g = std::tan (kPi * fc / fs);
        k = 1.0f / std::max (0.1f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void process (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

// RBJ biquad for the shelving EQ.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void shelf (bool high, float f, float dB, float fs)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f / fs, c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt (2.0f), sa = 2 * std::sqrt (A) * al;
        float B0, B1, B2, A0, A1, A2;
        if (! high) { B0 = A * ((A + 1) - (A - 1) * c + sa); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - sa);
                      A0 = (A + 1) + (A - 1) * c + sa; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - sa; }
        else        { B0 = A * ((A + 1) + (A - 1) * c + sa); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - sa);
                      A0 = (A + 1) - (A - 1) * c + sa; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - sa; }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct DcBlock { float x1 = 0, y1 = 0; float process (float x) { const float y = x - x1 + 0.995f * y1; x1 = x; y1 = y; return y; } };

struct DelayLine
{
    std::vector<float> buf; int w = 0;
    void init (int size) { buf.assign ((size_t) size, 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    // d = delay in samples (fractional), d >= 1
    float read (float d) const
    {
        const int n = (int) buf.size();
        d = std::clamp (d, 1.0f, (float) (n - 2));
        float r = (float) w - d; while (r < 0) r += (float) n;
        const int i = (int) r; const float f = r - (float) i;
        const float a = buf[(size_t) i], b = buf[(size_t) ((i + 1) % n)];
        return a + (b - a) * f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Band-limited basic shapes: 0 saw, 1 square/pulse, 2 triangle, 3 sine.
struct Osc
{
    float ph = 0, triState = 0;
    float next (int shape, float inc, float pw = 0.5f)
    {
        inc = std::clamp (inc, 0.0f, 0.49f);
        float y;
        switch (shape)
        {
            case 0: y = 2 * ph - 1 - polyBlep (ph, inc); break;
            case 1: case 2:
            {
                float sq = ph < pw ? 1.0f : -1.0f;
                sq += polyBlep (ph, inc);
                float t2 = ph - pw; if (t2 < 0) t2 += 1;
                sq -= polyBlep (t2, inc);
                if (shape == 1) { y = sq; break; }
                triState = 0.999f * triState + sq * inc * 4.0f;   // integrated square = triangle
                y = triState;
                break;
            }
            default: y = std::sin (kTwoPi * ph); break;
        }
        ph += inc; if (ph >= 1) ph -= 1;
        return y;
    }
};
} // namespace dsp
