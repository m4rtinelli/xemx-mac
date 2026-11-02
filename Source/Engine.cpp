#include "Engine.h"

using namespace emx;

void Engine::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    for (auto& pair : drums) for (auto& v : pair) v.prepare ((float) fs);
    for (auto& s : synths) s.prepare ((float) fs);
    for (auto& f : fx) f.prepare (fs);
    dry.setSize (2, maxBlock);
    for (auto& b : fxBus) b.setSize (2, maxBlock);
    inMono.setSize (1, maxBlock);
    events.reserve (1024);
    auditions.reserve (64);
    expectedPpq = -1;
}

bool Engine::audible (int part) const
{
    const auto solo = soloMask.load();
    if (solo) return (solo >> part) & 1;
    return ! ((muteMask.load() >> part) & 1);
}

void Engine::audition (int part, int note)
{
    const juce::SpinLock::ScopedLockType l (auditionLock);
    if (auditions.size() < 64) auditions.emplace_back (part, note);
}

void Engine::selectPattern (int index)
{
    index = juce::jlimit (0, kNumPatterns - 1, index);
    if (running) pendingPattern = index;
    else { currentPattern = index; seqPattern = index; expectedPpq = -1; }
}

void Engine::playSong (int songIndex)
{
    song = songIndex; songPos = 0;
    if (songIndex >= 0)
    {
        const juce::ScopedLock l (fileLock);
        auto songs = file.songs();
        if (songIndex < (int) songs.size() && ! songs[(size_t) songIndex].patterns.empty())
        {
            const int first = songs[(size_t) songIndex].patterns[0];
            currentPattern = first; seqPattern = first; pendingPattern = -1; expectedPpq = -1;
        }
    }
}

PartParams Engine::paramsAt (Pattern pat, int part, int idx) const
{
    PartParams p;
    const bool sy = Pattern::isSynth (part);
    const uint8_t* b = pat.part (part);
    const auto o = offsets (sy);
    p.level = b[o.level]; p.pan = b[o.pan]; p.egTime = b[o.egTime];
    const int f = b[o.flags], m = b[o.mod];
    p.fxSel = juce::jmin (2, f & 3); p.fxSend = (f >> 2) & 1; p.roll = (f >> 3) & 1; p.ampEG = (f >> 4) & 1;
    p.modDest = juce::jmin (5, m & 7); p.modType = juce::jmin (4, (m >> 4) & 7); p.bpmSync = m >> 7;
    p.modSpeed = b[o.speed]; p.modDepth = b[o.depth]; p.motion = b[o.motion] & 3;
    p.wave = b[0];
    if (sy)
    {
        p.tune = b[1] - 50; p.osc = b[2] & 15; p.edit1 = b[3]; p.edit2 = b[4]; p.glide = b[5];
        p.filter = b[6] & 3; p.cutoff = b[7]; p.reso = b[8]; p.egInt = b[9]; p.drive = b[10];
    }
    else p.pitch = b[1];

    if (p.motion == 0 || idx < 0) return p;
    MotionSlot slots[24];
    const int count = motionSlots (pat, slots);
    for (int si = 0; si < count; ++si)
    {
        const auto& slot = slots[si];
        if (slot.part != part) continue;
        int v = -1;
        for (int k = idx; k >= 0; --k) if (slot.values[k] != 0xff && slot.values[k] != 0x80) { v = slot.values[k]; break; }
        if (v < 0) continue;
        const bool on = v >= 64;
        if (! sy)
            switch (slot.param)
            {
                case 1: p.pitch = v; break; case 7: p.level = v; break; case 8: p.pan = v; break; case 9: p.egTime = v; break;
                case 10: p.ampEG = on; break; case 11: p.roll = on; break; case 13: p.fxSend = on; break;
                case 14: p.fxSel = juce::jmin (2, v / 43); break; case 16: p.modDepth = v; break; default: break;
            }
        else
            switch (slot.param)
            {
                case 3: p.edit1 = v; break; case 4: p.edit2 = v; break; case 5: p.glide = v; break; case 6: p.filter = juce::jmin (3, v >> 5); break;
                case 7: p.cutoff = v; break; case 8: p.reso = v; break; case 9: p.egInt = v; break; case 10: p.drive = v; break;
                case 11: p.level = v; break; case 12: p.pan = v; break; case 13: p.egTime = v; break; case 15: p.roll = on; break;
                case 17: p.fxSend = on; break; case 18: p.fxSel = juce::jmin (2, v / 43); break; case 19: p.modType = juce::jmin (4, v >> 4); break;
                case 20: p.modDepth = v; break; case 21: p.modSpeed = v; break; case 22: p.modDest = juce::jmin (5, v / 22); break; default: break;
            }
    }
    return p;
}

void Engine::doStep (Pattern pat, int si, juce::int64 t0, float stepSamples)
{
    const int last = pat.lastStep(), within = si % last, meas = si / last, idx = meas * 16 + within;
    playStep = within; playMeasure = meas;

    // effect edits from motion sequences
    MotionSlot slots[24];
    const int count = motionSlots (pat, slots);
    for (int m = 0; m < count; ++m)
    {
        const auto& slot = slots[m];
        if (slot.part != 14) continue;
        const int k = slot.param == 5 || slot.param == 6 ? 0 : slot.param == 9 || slot.param == 10 ? 1 : slot.param == 13 || slot.param == 14 ? 2 : -1;
        const int v = slot.values[idx];
        if (k >= 0 && (pat.fx (k)[3] & 1) && v != 0xff && v != 0x80)
            events.push_back ({ t0, kFxMotion, k, (slot.param & 1) ? 0 : 1, (float) v, 0, {} });
    }

    const float swingOff = (within & 1) ? (pat.swing() / 50.0f - 1.0f) * stepSamples : 0.0f;
    const int rolls = pat.rolls();
    for (int part = 0; part < kNumParts; ++part)
    {
        const bool sy = Pattern::isSynth (part);
        const int note = sy ? pat.note (part - kNumDrums, idx) : (pat.drumStep (part, idx) ? 0 : -1);
        if (note < 0)
        {
            if (sy && (pat.synth (part - kNumDrums)[19] & 3)) events.push_back ({ t0, kSynthMotion, part, 0, 0, 0, paramsAt (pat, part, idx) });
            continue;
        }
        if (! audible (part)) continue;
        const auto p = paramsAt (pat, part, idx);
        const juce::int64 t = t0 + (pat.swingOn (part) ? (juce::int64) swingOff : 0);
        const float acc = pat.accentOn (part) && pat.accentStep (sy, idx) ? 1.0f + pat.accentLevel (sy) / 127.0f * (sy ? 0.6f : 0.8f) : 1.0f;
        const int hits = p.roll ? rolls : 1;
        const float gate = gateSteps (sy ? pat.gateByte (part - kNumDrums, idx) : 0) * stepSamples;
        for (int r = 0; r < hits; ++r)
            events.push_back ({ t + (juce::int64) (r * stepSamples / hits), sy ? kSynthNote : kDrumHit, part, note,
                                acc * (r ? 0.8f : 1.0f), hits > 1 ? juce::jmin (gate, 0.9f * stepSamples / hits) : gate, p });
    }
}

void Engine::onPatternEnd()
{
    const int s = song.load();
    if (s >= 0)
    {
        auto songs = file.songs();
        if (s < (int) songs.size() && ! songs[(size_t) s].patterns.empty())
        {
            auto& list = songs[(size_t) s].patterns;
            const int pos = (songPos.load() + 1) % (int) list.size();
            songPos = pos;
            seqPattern = list[(size_t) pos];
        }
    }
    else if (pendingPattern.load() >= 0) { seqPattern = pendingPattern.exchange (-1); }
    if (currentPattern.load() != seqPattern) memset (fxEdit, 0, sizeof (fxEdit));
    currentPattern = seqPattern;
}

void Engine::scanSteps (Pattern pat, double ppqStart, double ppqEnd, double samplesPerPpq)
{
    double step = stepPpq (pat.beat());
    int total = pat.lastStep() * pat.measures();
    if (expectedPpq < 0 || std::abs (ppqStart - expectedPpq) > 0.01)
    {
        // (re)locate inside the pattern, aligned to the host's bar grid
        const double len = step * total, pos = std::fmod (std::max (0.0, ppqStart), len);
        stepIdx = (int) std::ceil (pos / step - 1e-6);
        nextStepPpq = ppqStart + (stepIdx * step - pos);
        if (stepIdx >= total) stepIdx = 0;
    }
    while (nextStepPpq < ppqEnd)
    {
        const juce::int64 t = clock + (juce::int64) std::max (0.0, (nextStepPpq - ppqStart) * samplesPerPpq);
        doStep (pat, stepIdx, t, (float) (step * samplesPerPpq));
        nextStepPpq += step;
        if (++stepIdx >= total)
        {
            stepIdx = 0;
            onPatternEnd();
            pat = file.pattern (seqPattern);
            step = stepPpq (pat.beat());
            total = pat.lastStep() * pat.measures();
        }
    }
    expectedPpq = ppqEnd;
}

void Engine::applyEvent (const Event& e)
{
    const auto bank = samples.current();
    switch (e.kind)
    {
        case kDrumHit:
        {
            auto& slot = drums[e.part][drumNext[e.part]];
            drums[e.part][drumNext[e.part] ^ 1].release();
            drumNext[e.part] ^= 1;
            slot.trigger (e.p, e.gain, (*bank)[(size_t) juce::jlimit (0, 206, e.p.wave)], e.p.wave, (float) bar);
            partFlash |= 1u << e.part;
            break;
        }
        case kSynthNote:
            synths[e.part - kNumDrums].noteOn (e.p, e.note, e.gate, e.gain, (float) bar);
            partFlash |= 1u << e.part;
            break;
        case kSynthMotion:
            if (synths[e.part - kNumDrums].active()) synths[e.part - kNumDrums].update (e.p);
            break;
        case kSynthOff:
            if (synths[e.part - kNumDrums].lastNote() == e.note) synths[e.part - kNumDrums].noteOff();
            break;
        case kFxMotion:
            fxEdit[e.part][e.note] = (int) e.gain;
            break;
        default: break;
    }
}

void Engine::renderVoices (int from, int to, const float* audioIn)
{
    const int n = to - from;
    if (n <= 0) return;
    auto bus = [&] (const PartParams& p, int ch) { return (p.fxSend ? fxBus[p.fxSel] : dry).getWritePointer (ch) + from; };
    for (auto& pair : drums)
        for (auto& v : pair)
            if (v.active()) v.render (bus (v.params, 0), bus (v.params, 1), n);
    for (auto& s : synths)
        if (s.active()) s.render (bus (s.params, 0), bus (s.params, 1), n, audioIn ? audioIn + from : nullptr);
}

void Engine::process (juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>* in, const juce::MidiBuffer& midi, const Transport& tr)
{
    const int n = out.getNumSamples();
    out.clear();
    const juce::ScopedTryLock lock (fileLock);
    if (! lock.isLocked() || n > dry.getNumSamples()) return;

    dry.clear(); for (auto& b : fxBus) b.clear();
    const float* audioIn = nullptr;
    if (in && in->getNumChannels() > 0)
    {
        inMono.copyFrom (0, 0, *in, 0, 0, n);
        if (in->getNumChannels() > 1) { inMono.addFrom (0, 0, *in, 1, 0, n); inMono.applyGain (0, 0, n, 0.5f); }
        inMono.applyGain (0, 0, n, audioInLevel.load() / 127.0f);
        audioIn = inMono.getReadPointer (0);
    }

    auto pat = file.pattern (seqPattern);
    const bool hostRun = tr.hostPlaying, intRun = ! hostRun && internalPlay.load();
    const double bpm = intRun && tempoLock.load() ? pat.bpm() : tr.bpm;
    displayBpm = intRun && tempoLock.load() ? pat.bpm() : (hostRun ? tr.bpm : pat.bpm());
    bar = 240.0 / juce::jmax (20.0, bpm) * fs;
    for (auto& f : fx) f.setBpm (bpm);
    const double samplesPerPpq = 60.0 / juce::jmax (20.0, bpm) * fs;

    // MIDI: notes play the selected part (synth parts follow the pitch); program change picks a pattern
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int part = selectedPart.load();
        if (m.isProgramChange()) selectPattern (m.getProgramChangeNumber());
        else if (m.isNoteOn())
            events.push_back ({ clock + meta.samplePosition, Pattern::isSynth (part) ? kSynthNote : kDrumHit, part, m.getNoteNumber(),
                                m.getFloatVelocity() * 1.25f, (float) (bar * 8), paramsAt (pat, part, -1) });
        else if (m.isNoteOff() && Pattern::isSynth (part))
            events.push_back ({ clock + meta.samplePosition, kSynthOff, part, m.getNoteNumber(), 0, 0, {} });
    }

    const bool run = hostRun || intRun;
    if (stopRequest.exchange (false) || (wasRunning && ! run))
    {
        for (auto& pair : drums) for (auto& v : pair) v.release();
        for (auto& s : synths) s.noteOff();
        events.clear(); playStep = -1; expectedPpq = -1;
    }
    if (run && ! wasRunning) { expectedPpq = -1; if (intRun) internalPpq = 0; }
    wasRunning = run; running = run;

    if (run)
    {
        const double start = hostRun ? tr.ppq : internalPpq, end = start + n / samplesPerPpq;
        scanSteps (pat, start, end, samplesPerPpq);
        if (intRun) internalPpq = end;
        beatPhase = (float) std::fmod (start, 1.0);
    }
    else if (pendingPattern.load() >= 0) { seqPattern = pendingPattern.exchange (-1); currentPattern = seqPattern; }

    {   // pads, keyboard mode and MIDI notes play immediately
        const juce::SpinLock::ScopedLockType l (auditionLock);
        auto now = file.pattern (seqPattern);
        for (auto [part, note] : auditions)
            events.push_back ({ clock, Pattern::isSynth (part) ? kSynthNote : kDrumHit, part, note, 1.0f, 0.5f * (float) bar / 16, paramsAt (now, part, -1) });
        auditions.clear();
    }

    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    int pos = 0;
    size_t used = 0;
    for (; used < events.size(); ++used)
    {
        const auto& e = events[used];
        const int at = (int) juce::jlimit<juce::int64> (0, n, e.time - clock);
        if (at >= n) break;
        renderVoices (pos, at, audioIn);
        pos = at;
        applyEvent (e);
    }
    renderVoices (pos, n, audioIn);
    events.erase (events.begin(), events.begin() + (long) used);

    // effects and chain: 0 off, 1 = 1>2, 2 = 2>3, 3 = 1>2>3
    const int chain = pat.fxChain();
    const int next[4][3] { { -1, -1, -1 }, { 1, -1, -1 }, { -1, 2, -1 }, { 1, 2, -1 } };
    for (int k = 0; k < 3; ++k)
    {
        const uint8_t* f = pat.fx (k);
        const bool mot = f[3] & 1;
        fx[k].configure (f[0] & 15, mot && fxEdit[k][0] ? fxEdit[k][0] : f[1], mot && fxEdit[k][1] ? fxEdit[k][1] : f[2]);
        fx[k].process (fxBus[k].getWritePointer (0), fxBus[k].getWritePointer (1), n);
        auto& dest = next[chain][k] >= 0 ? fxBus[next[chain][k]] : dry;
        for (int c = 0; c < 2; ++c) dest.addFrom (c, 0, fxBus[k], c, 0, n);
    }
    if (audioThru.load() && audioIn) for (int c = 0; c < 2; ++c) dry.addFrom (c, 0, inMono, 0, 0, n);

    // valve stage (TUBE GAIN) and MASTER VOLUME
    const float drive = 1.0f + tubeGain.load() / 127.0f * 7.0f, bias = 0.22f, comp = 1.0f / std::sqrt (drive);
    const float lpk = 1.0f - std::exp (-dsp::kTwoPi * (16000.0f - drive * 900.0f) / (float) fs);
    const float mv = std::pow (masterVolume.load() / 127.0f, 2.0f) * 1.3f;
    const float tb = std::tanh (bias);
    float peak = 0;
    for (int c = 0; c < juce::jmin (2, out.getNumChannels()); ++c)
    {
        const float* x = dry.getReadPointer (c);
        float* y = out.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float v = (std::tanh (x[i] * drive + bias) - tb) * comp * 1.2f;
            v = dc[c].process (v);
            valveLp[c] += (v - valveLp[c]) * lpk;
            y[i] = valveLp[c] * mv;
            peak = std::max (peak, std::abs (y[i]));
        }
    }
    if (out.getNumChannels() > 2) for (int c = 2; c < out.getNumChannels(); ++c) out.clear (c, 0, n);
    meter = std::max (peak, meter * 0.9f);
    outputLevel = meter;
    clock += n;
}
