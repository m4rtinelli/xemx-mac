#include "PluginEditor.h"

using namespace ui;
using emx::Pattern;

namespace
{
const char* const kItems[6][7] = {
    { "PATTERN", "TEMPO", "SWING", "ROLL TYPE", "LENGTH", "BEAT", "ARPEGGIO SCALE" },
    { "WAVE", "SYNTH TUNE", "OUTPUT BUS", "SWING SW", "ACCENT SW" },
    { "MOTION DEST", "MOTION VALUE", "GATE TIME", "NOTE NO.", "STEP REC" },
    { "SONG", "TEMPO", "POSITION", "PATTERN", "NOTE OFFSET", "NOTE HOLD", "NEXT SONG" },
    { "METRONOME", "TAP CONTROL", "MASTER TUNE", "PROTECT" },
    { "CLOCK", "MIDI CH", "BEND RANGE" } };
const int kItemCount[6] { 7, 5, 5, 7, 4, 3 };
const char* const kModeNames[6] { "PATTERN", "PART EDIT", "STEP EDIT", "SONG", "GLOBAL", "MIDI" };
const char* const kStepLabels[16][2] { { "LAST", "STEP" }, { "MOVE", "DATA" }, { "SHIFT", "NOTE" }, { "COPY", "PART" }, { "COPY", "SOUND" },
    { "CLEAR", "MOTION" }, { "CLEAR", "PART" }, { "CLEAR", "PATTERN" }, { "INSERT", "PATTERN" }, { "DELETE", "PATTERN" }, { "CLEAR", "SONG" },
    { "MIDI", "UTILITY" }, { "CARD", "" }, { "", "" }, { "", "" }, { "", "" } };
const char* const kBeats[4] { "16", "32", "8TRI", "16TRI" };
const char* const kNotes[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const std::vector<std::vector<int>> kScaleSteps {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 6, 7, 9, 11 },
    { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 6, 8, 10 }, { 0, 2, 3, 4, 7, 9 }, { 0, 3, 5, 6, 7, 10 }, { 0, 2, 3, 5, 6, 8, 9, 11 },
    { 0, 1, 3, 4, 6, 7, 9, 10 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 1, 4, 5, 7, 8, 11 }, { 0, 1, 4, 6, 7, 9, 11 }, { 0, 1, 3, 6, 7, 8, 11 },
    { 0, 1, 3, 4, 5, 7, 8, 10 }, { 0, 2, 3, 6, 7, 8, 11 }, { 0, 2, 4, 5, 6, 8, 10 }, { 0, 2, 5, 7, 10 }, { 0, 2, 3, 7, 9 }, { 0, 1, 3, 7, 8 },
    { 0, 1, 5, 7, 8 }, { 0, 4, 5, 7, 11 }, { 0, 2, 4, 6, 8, 10 }, { 0, 3, 6, 9 }, { 0, 4, 8 }, { 0, 5, 10 }, { 0, 7 }, { 0 } };

juce::String noteName (int n) { return n < 0 ? "--" : juce::String (kNotes[n % 12]) + juce::String (n / 12 - 1); }
juce::String sgn (int v) { v = emx::signed64 (v); return v > 0 ? "+" + juce::String (v) : juce::String (v); }
juce::String panStr (int v) { v -= 64; return v == 0 ? "C" : v < 0 ? "L" + juce::String (-v) : "R" + juce::String (v); }
juce::String num (int v) { return juce::String (v); }

void text (juce::Graphics& g, const juce::String& t, float x, float y, float w, float h, float mm, juce::Justification j = juce::Justification::centred,
           juce::Colour c = col::label, bool italic = true)
{
    g.setColour (c);
    auto f = font (mm);
    g.setFont (italic ? f.italicised() : f);
    g.drawText (t, juce::Rectangle<float> (x, y, w, h), j, false);
}
} // namespace

// ---------------------------------------------------------------- LCD
class Lcd : public Widget
{
public:
    explicit Lcd (XemxEditor& e) : ed (e) { state = [this] { auto l = lines(); return (juce::int64) (l[0] + "|" + l[1] + "|" + l[2]).hashCode64(); }; }
    juce::StringArray lines()
    {
        auto& eng = ed.eng;
        if (juce::Time::getMillisecondCounter() < ed.msgUntil) return { ed.msgA, ed.msgB, "" };
        auto p = ed.pat();
        juce::String top = ed.mode == XemxEditor::modeSong && eng.song.load() >= 0 ? eng.file.songs()[(size_t) eng.song.load()].name : p.name();
        juce::String bpm = juce::String (eng.displayBpm.load(), 1);
        juce::String it = juce::String (kItems[ed.mode][ed.item[ed.mode]]) + " " + ed.itemValue (ed.mode, ed.item[ed.mode]);
        return { top.isEmpty() ? "(empty)" : top, it, bpm };
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::lcdBg); g.fillRoundedRectangle (b, 0.8f * s);
        g.setColour (col::lcdInk.withAlpha (0.08f));
        for (float y = 0; y < b.getHeight(); y += 0.6f * s) g.drawHorizontalLine ((int) y, 0, b.getWidth());
        auto l = lines();
        g.setColour (col::lcdInk);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 7.2f * s, juce::Font::bold)));
        g.drawText (l[0], b.reduced (2.2f * s, 1.5f * s).withHeight (9 * s), juce::Justification::centredLeft, true);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 4.6f * s, juce::Font::bold)));
        g.drawText (l[2], b.reduced (2.2f * s, 1.5f * s).withHeight (9 * s), juce::Justification::centredRight, true);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 3.6f * s, juce::Font::plain)));
        g.drawText (l[1], b.reduced (2.2f * s, 1.5f * s).removeFromBottom (13 * s).removeFromTop (6 * s), juce::Justification::centredLeft, true);
        juce::String info = juce::String::formatted ("P%03d  ", ed.eng.currentPattern.load() + 1) + kModeNames[ed.mode] + "  "
                          + juce::String (ed.pat().measures()) + " MEAS  " + kBeats[ed.pat().beat()];
        g.drawText (info, b.reduced (2.2f * s, 1.5f * s).removeFromBottom (6 * s), juce::Justification::centredLeft, true);
    }
    XemxEditor& ed;
};

// ---------------------------------------------------------------- arpeggiator touch pad
class TouchPad : public Widget
{
public:
    explicit TouchPad (XemxEditor& e) : ed (e) { state = [this] { return (juce::int64) ed.arpNote + 1000; }; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::knob); g.fillRoundedRectangle (b, 1.2f * s);
        g.setColour (col::line); g.drawRoundedRectangle (b.reduced (0.3f * s), 1.2f * s, 0.35f * s);
        g.setColour (col::panelHi); g.fillRect (b.reduced (2.2f * s));
        g.setColour (col::line.withAlpha (0.3f));
        for (int i = 1; i < 8; ++i) g.drawHorizontalLine ((int) (b.getY() + 2.2f * s + (b.getHeight() - 4.4f * s) * i / 8), b.getX() + 2.2f * s, b.getRight() - 2.2f * s);
        if (ed.arpNote >= 0) { g.setColour (col::accent.withAlpha (0.6f)); g.fillEllipse (pos.x - 2.5f * s, pos.y - 2.5f * s, 5 * s, 5 * s); }
    }
    void mouseDown (const juce::MouseEvent& e) override { update (e); ed.arpCounter = 0; }
    void mouseDrag (const juce::MouseEvent& e) override { update (e); }
    void mouseUp (const juce::MouseEvent&) override { ed.arpNote = -1; repaint(); }
    void update (const juce::MouseEvent& e)
    {
        pos = e.position;
        // vertical = scale degree (top = high), like the EMX ribbon
        auto p = ed.pat();
        const auto& sc = kScaleSteps[(size_t) p.scale()];
        const int degree = juce::jlimit (0, 15, (int) ((1.0f - e.position.y / (float) getHeight()) * 16));
        ed.arpNote = juce::jlimit (0, 127, p.arpCentre() - 12 + 12 * (degree / (int) sc.size()) + sc[(size_t) (degree % (int) sc.size())]);
        repaint();
    }
    juce::Point<float> pos;
    XemxEditor& ed;
};

// ---------------------------------------------------------------- editor
XemxEditor::XemxEditor (XemxProcessor& p) : AudioProcessorEditor (p), proc (p), eng (p.engine)
{
    build();
    constrainer.setFixedAspectRatio (kWidthMm / kHeightMm);
    constrainer.setMinimumSize ((int) (kWidthMm * 2.2f), (int) (kHeightMm * 2.2f));
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) (kWidthMm * 3.2f), (int) (kHeightMm * 3.2f));
    proc.onFileChanged = [sp = juce::Component::SafePointer<XemxEditor> (this)]
    {
        juce::MessageManager::callAsync ([sp] { if (sp) { sp->message ("CARD", sp->proc.emxName()); sp->repaint(); } });
    };
    startTimerHz (30);
}

XemxEditor::~XemxEditor() { proc.onFileChanged = nullptr; }

void XemxEditor::message (const juce::String& a, const juce::String& b)
{
    msgA = a; msgB = b; msgUntil = juce::Time::getMillisecondCounter() + 1400;
}

juce::String XemxEditor::partLabel (int p) const { return p < emx::kNumDrums ? "DRUM " + juce::String (emx::kPartNames[p]) : "SYNTH " + juce::String (p - 8); }

ui::Knob* XemxEditor::knob (float cx, float cy, float ringMm, juce::String name, std::function<int()> get, std::function<void (int)> set,
                            std::function<juce::String (int)> fmt, std::function<bool()> enabled, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->ringMm = ringMm; k->bipolar = bipolar; k->get = get;
    if (enabled) k->enabled = enabled;
    k->set = [this, set, fmt, name, get] (int v) { set (v); message (name, fmt (get())); };
    k->touched = [this, fmt, name, get] { message (name, fmt (get())); };
    auto* raw = add (std::move (k), cx, cy, ringMm * 2 + 3, ringMm * 2 + 3);
    widgets.push_back (raw);
    return raw;
}

ui::Button* XemxEditor::button (float cx, float cy, float w, float h, juce::String t, std::function<void()> click, std::function<int()> lit)
{
    auto b = std::make_unique<Button>();
    b->text = t; b->click = click;
    if (lit) b->lit = lit;
    auto* raw = add (std::move (b), cx, cy, w, h);
    widgets.push_back (raw);
    return raw;
}

ui::Led* XemxEditor::led (float cx, float cy, std::function<int()> lit, float d)
{
    auto l = std::make_unique<Led>();
    l->lit = lit;
    auto* raw = add (std::move (l), cx, cy, d, d);
    widgets.push_back (raw);
    return raw;
}

void XemxEditor::build()
{
    auto sy = [this] { return synthSel(); };
    auto& P = proc.params;
    auto paramKnob = [&] (float cx, float cy, const char* id, const char* name)
    {
        auto* prm = P.getParameter (id);
        return knob (cx, cy, 7.0f, name, [this, id] { return (int) proc.params.getRawParameterValue (id)->load(); },
                     [prm] (int v) { prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); }, num);
    };

    // ---- top strip: card functions of the plugin
    button (31, 8, 20, 5.5f, "LOAD .EMX", [this] { openEmx(); });
    button (54, 8, 20, 5.5f, "SAVE .EMX", [this] { saveEmx(); });
    button (77, 8, 20, 5.5f, "SAMPLES", [this] { chooseSamples(); }, [this] { return proc.sampleState.load() == 2 ? 0 : 0; });

    // ---- volume section
    paramKnob (29.5f, 50, "master", "MASTER VOLUME");
    paramKnob (57, 50, "tube", "TUBE GAIN");
    paramKnob (97.5f, 50, "audioIn", "AUDIO IN LEVEL");

    // ---- LCD block
    auto lcd = std::make_unique<Lcd> (*this);
    widgets.push_back (add (std::move (lcd), 57.5f, 86.5f, 52, 27));
    for (int i = 0; i < emx::kNumParts; ++i)
        led (i < 9 ? 34.3f + i * 3.2f : 66.0f + (i - 9) * 3.2f, 105.5f, [this, i] { return part() == i ? 2 : ((eng.partFlash.load() >> i) & 1); }, 1.8f);
    button (110, 70, 15, 6, "AUDIO IN THRU", [this] { eng.audioThru = ! eng.audioThru.load(); }, [this] { return (int) eng.audioThru.load(); });
    button (110, 80, 15, 6, "TEMPO LOCK", [this] { eng.tempoLock = ! eng.tempoLock.load(); }, [this] { return (int) eng.tempoLock.load(); });
    button (110, 90, 15, 6, "AUTO BPM SCAN", [this] { message ("AUTO BPM", "needs audio in"); });
    button (110, 103.5f, 15, 6, "WRITE", [this] { saveEmx(); });
    led (125, 80, [this] { return eng.running.load() && eng.beatPhase.load() < 0.2f ? 2 : 0; });
    auto j = std::make_unique<Jog>();
    j->turned = [this] (int d) { jog (d); };
    add (std::move (j), 141, 89, 23, 23);

    button (26, 116.5f, 12, 7.5f, "UP", [this] { item[mode] = (item[mode] + kItemCount[mode] - 1) % kItemCount[mode]; });
    button (26, 128.5f, 12, 7.5f, "DOWN", [this] { item[mode] = (item[mode] + 1) % kItemCount[mode]; });
    const float modeX[6] { 46.5f, 67.5f, 88.5f, 108.7f, 128.2f, 151.5f };
    for (int m = 0; m < 6; ++m)
        button (modeX[m], 139, 18, 6.5f, kModeNames[m], [this, m]
        {
            mode = m;
            if (m == modeSong) eng.playSong (song); else if (eng.song.load() >= 0) eng.playSong (-1);
        }, [this, m] { return (int) (mode == m); });

    // ---- transport
    auto rew = button (26.5f, 155, 18, 12.5f, "", [this] { eng.stopAll(); eng.selectPattern (eng.currentPattern.load()); viewMeasure = 0; });
    rew->drawing = "rew";
    auto recB = button (48, 155, 18, 12.5f, "", [this] { rec = ! rec; }, [this] { return (int) rec; });
    recB->drawing = "rec";
    auto stopB = button (70, 155, 18, 12.5f, "", [this] { eng.internalPlay = false; eng.stopAll(); follow = true; });
    stopB->drawing = "stop";
    auto playB = button (99, 155, 30, 12.5f, "", [this] { eng.internalPlay = ! eng.internalPlay.load(); follow = true; },
                         [this] { return eng.running.load() ? (eng.beatPhase.load() < 0.5f ? 2 : 1) : 0; });
    playB->drawing = "play";
    auto tap = button (131, 157.5f, 17, 17, "TAP", [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.isEmpty() && now - taps.getLast() > 2000) taps.clear();
        taps.add (now);
        if (taps.size() >= 3)
        {
            const double bpm = 60000.0 * (taps.size() - 1) / (taps.getLast() - taps.getFirst());
            pat().setBpm (bpm); message ("TEMPO", juce::String (bpm, 1));
            if (taps.size() > 6) taps.remove (0);
        }
    });
    tap->textMm = 3.4f;
    button (152, 157.5f, 9.5f, 17, "PART MUTE", [this] { muteLatch = ! muteLatch; soloLatch = false; }, [this] { return (int) muteLatch; });
    button (168.5f, 157.5f, 9.5f, 17, "SOLO", [this] { soloLatch = ! soloLatch; muteLatch = false; }, [this] { return (int) soloLatch; });

    // ---- effect
    auto fxByte = [this] (int i) -> uint8_t& { return pat().fx (fxSlot)[i]; };
    auto* fk = knob (206.4f, 89.3f, 11.8f, "EFFECT", [fxByte] { return fxByte (0) & 15; }, [fxByte] (int v) { fxByte (0) = (uint8_t) v; },
                     [] (int v) { return juce::String (emx::kFxTypes[v & 15]); });
    fk->positions = 16; fk->maxV = 15; fk->startAngle = -juce::MathConstants<float>::pi; fk->endAngle = juce::MathConstants<float>::pi * 7 / 8;
    auto fxEditFmt = [this, fxByte] (int k) { return [this, fxByte, k] (int v) -> juce::String
    {
        const int t = fxByte (0) & 15;
        if (k == 1 && (t == 1 || t == 3)) return emx::delayNoteName (v);
        if (k == 1 && t == 9) return juce::String ((int) std::round ((v - 64) * 0.3f) * 100) + " cent";
        if (t == 13 || (t == 8 && k == 2)) return sgn (v);
        return num (v);
    }; };
    knob (192, 118.7f, 7.0f, "FX EDIT 1", [fxByte] { return (int) fxByte (1); }, [fxByte] (int v) { fxByte (1) = (uint8_t) v; }, fxEditFmt (1));
    knob (219.7f, 118.7f, 7.0f, "FX EDIT 2", [fxByte] { return (int) fxByte (2); }, [fxByte] (int v) { fxByte (2) = (uint8_t) v; }, fxEditFmt (2));
    for (int k = 0; k < 3; ++k) led (211.5f, 132.0f + k * 6, [this, k] { return (int) (fxSlot == k); });
    led (199.5f, 139.4f, [this] { const int c = pat().fxChain(); return (int) (c == 1 || c == 3); }, 2.0f);
    led (199.5f, 145.5f, [this] { const int c = pat().fxChain(); return (int) (c == 2 || c == 3); }, 2.0f);
    button (192, 163, 17, 6.3f, "FX CHAIN", [this] { auto& b = pat().p[0x0c]; b = (uint8_t) ((b & ~3) | ((b + 1) & 3));
        const char* names[] { "OFF", "1>2", "2>3", "1>2>3" }; message ("FX CHAIN", names[b & 3]); });
    button (219.7f, 152, 17, 6.3f, "EDIT SELECT", [this] { fxSlot = (fxSlot + 1) % 3; message ("EDIT FX" + juce::String (fxSlot + 1), emx::kFxTypes[pat().fx (fxSlot)[0] & 15]); });
    button (219.7f, 163, 17, 6.3f, "MOTION SEQ", [this, fxByte] { fxByte (3) ^= 1; message ("FX MOTION", fxByte (3) & 1 ? "ON" : "OFF"); },
            [fxByte] { return fxByte (3) & 1; });

    // ---- part common
    knob (246, 45, 7.0f, "PITCH/GLIDE", [this] { return (int) partByte (1, 5); }, [this] (int v) { partByte (1, 5) = (uint8_t) v; },
          [this] (int v) { return synthSel() ? "GLIDE " + num (v) : "PITCH " + sgn (v); })->bipolar = true;
    knob (274, 45, 7.0f, "PAN", [this] { return (int) partByte (3, 12); }, [this] (int v) { partByte (3, 12) = (uint8_t) v; }, panStr)->bipolar = true;
    knob (246, 68, 7.0f, "EG TIME", [this] { return (int) partByte (4, 13); }, [this] (int v) { partByte (4, 13) = (uint8_t) v; }, num);
    knob (274, 68, 7.0f, "LEVEL", [this] { return (int) partByte (2, 11); }, [this] (int v) { partByte (2, 11) = (uint8_t) v; }, num);
    auto flags = [this] () -> uint8_t& { return partByte (5, 14); };
    auto modB = [this] () -> uint8_t& { return partByte (6, 16); };
    led (238.2f, 75.5f, [flags] { return (flags() >> 4) & 1; }, 2.0f);
    led (238.2f, 81.5f, [flags] { return ! ((flags() >> 4) & 1); }, 2.0f);
    for (int k = 0; k < 3; ++k) led (258.3f, 69.5f + k * 6, [flags, k] { return (int) ((flags() & 3) == k); });
    button (248, 89.5f, 15, 6.5f, "AMP EG", [flags] { flags() ^= 0x10; }, [flags] { return (flags() >> 4) & 1; });
    button (266.2f, 89.5f, 15, 6.5f, "FX SELECT", [flags] { auto& f = flags(); f = (uint8_t) ((f & ~3) | (((f & 3) + 1) % 3)); });
    button (248, 99.5f, 15, 6.5f, "ROLL", [flags] { flags() ^= 0x08; }, [flags] { return (flags() >> 3) & 1; });
    button (266.2f, 99.5f, 15, 6.5f, "FX SEND", [flags] { flags() ^= 0x04; }, [flags] { return (flags() >> 2) & 1; });
    auto motionB = [this] () -> uint8_t& { return partByte (9, 19); };
    led (280.5f, 84.7f, [motionB] { return (int) ((motionB() & 3) == 1); });
    led (280.5f, 90.7f, [motionB] { return (int) ((motionB() & 3) == 2); });
    button (288.7f, 99.5f, 18.5f, 6, "MOTION SEQ", [this, motionB] { auto& m = motionB(); m = (uint8_t) ((m & ~3) | (((m & 3) + 1) % 3));
        const char* n[] { "OFF", "SMOOTH", "TRIG HOLD" }; message ("MOTION SEQ", n[m & 3]); });

    // ---- modulation
    knob (302, 45, 7.0f, "SPEED", [this] { return (int) partByte (7, 17); }, [this] (int v) { partByte (7, 17) = (uint8_t) v; },
          [modB] (int v) { return (modB() & 0x80) ? juce::String (emx::kSyncNames[juce::jlimit (0, 16, (int) std::round (v * 16 / 127.0))]) : num (v); });
    knob (330, 45, 7.0f, "DEPTH", [this] { return (int) partByte (8, 18); }, [this] (int v) { partByte (8, 18) = (uint8_t) v; }, sgn)->bipolar = true;
    for (int k = 0; k < 5; ++k) led (302.4f, 57.5f + k * 6, [modB, k] { return (int) (((modB() >> 4) & 7) == k); });
    const int dests[6] { 0, 1, 2, 3, 4, 5 };
    for (int k = 0; k < 6; ++k) led (325, 57.5f + k * 6, [modB, k = dests[k]] { return (int) ((modB() & 7) == k); });
    button (309.7f, 89.5f, 15, 6.5f, "TYPE", [modB, this] { auto& m = modB(); m = (uint8_t) ((m & 0x8f) | (((((m >> 4) & 7) + 1) % 5) << 4));
        const char* n[] { "SAW", "SQUARE", "TRIANGLE", "S&H", "EG" }; message ("MOD TYPE", n[(m >> 4) & 7]); });
    button (309.7f, 99.5f, 15, 6.5f, "BPM SYNC", [modB] { modB() ^= 0x80; }, [modB] { return (modB() >> 7) & 1; });
    button (330.7f, 99.5f, 15, 6.5f, "DEST", [modB, sy, this]
    {
        auto& m = modB(); int d = m & 7;
        if (sy()) d = (d + 1) % 6; else d = d == 0 ? 4 : d == 4 ? 5 : 0;
        m = (uint8_t) ((m & ~7) | d);
        const char* n[] { "PITCH", "OSC EDIT 1", "OSC EDIT 2", "CUTOFF", "AMP", "PAN" }; message ("MOD DEST", n[d]);
    });

    // ---- synth oscillator / filter (synth parts only)
    auto synthByte = [this] (int off) -> uint8_t& { static uint8_t dummy; return synthSel() ? partBytes()[off] : (dummy = 0); };
    auto* ok = knob (260, 131.5f, 11.8f, "OSC TYPE", [synthByte] { return synthByte (2) & 15; },
                     [this, synthByte] (int v) { synthByte (2) = (uint8_t) v; auto n = emx::waveNames (v).size(); if (n) synthByte (0) = (uint8_t) juce::jmin ((int) synthByte (0), n - 1); },
                     [] (int v) { return juce::String (emx::kOscTypes[v & 15]); }, sy);
    ok->positions = 16; ok->maxV = 15; ok->startAngle = -juce::MathConstants<float>::pi * 7 / 8; ok->endAngle = juce::MathConstants<float>::pi;
    knob (246, 157.7f, 7.0f, "OSC EDIT 1", [synthByte] { return (int) synthByte (3); }, [synthByte] (int v) { synthByte (3) = (uint8_t) v; }, num, sy);
    knob (273.7f, 157.7f, 7.0f, "OSC EDIT 2", [synthByte] { return (int) synthByte (4); }, [synthByte] (int v) { synthByte (4) = (uint8_t) v; }, num, sy)->bipolar = true;
    knob (302, 119.8f, 7.0f, "CUTOFF", [synthByte] { return (int) synthByte (7); }, [synthByte] (int v) { synthByte (7) = (uint8_t) v; }, num, sy);
    knob (330, 119.8f, 7.0f, "RESONANCE", [synthByte] { return (int) synthByte (8); }, [synthByte] (int v) { synthByte (8) = (uint8_t) v; }, num, sy);
    knob (302, 145.7f, 7.0f, "EG INT", [synthByte] { return (int) synthByte (9); }, [synthByte] (int v) { synthByte (9) = (uint8_t) v; }, sgn, sy, true);
    knob (330, 145.7f, 7.0f, "DRIVE", [synthByte] { return (int) synthByte (10); }, [synthByte] (int v) { synthByte (10) = (uint8_t) v; }, num, sy);
    button (302, 162.5f, 15, 6.5f, "TYPE", [this, synthByte] { if (! synthSel()) return; auto& f = synthByte (6); f = (uint8_t) ((f + 1) & 3);
        const char* n[] { "LPF", "HPF", "BPF", "BPF+" }; message ("FILTER", n[f & 3]); });
    for (int k = 0; k < 4; ++k) led (k % 2 ? 327.0f : 316.0f, k < 2 ? 160.0f : 166.0f, [this, synthByte, k] { return (int) (synthSel() && (synthByte (6) & 3) == k); });

    // ---- arpeggiator
    auto pad = std::make_unique<TouchPad> (*this);
    widgets.push_back (add (std::move (pad), 34.75f, 209.75f, 21.5f, 60.5f));

    // ---- parts
    for (int i = 0; i < emx::kNumParts; ++i)
    {
        const float x = i < 9 ? 92.0f + 16 * i : 252.4f + 16 * (i - 9);
        auto* b = button (x, 186, 13.5f, 20, emx::kPartNames[i < 9 ? i : 0], [this, i] { padPressed (i); },
                          [this, i]
                          {
                              if (muteLatch || soloLatch) return eng.soloMask.load() ? (int) ((eng.soloMask.load() >> i) & 1) : (int) ! ((eng.muteMask.load() >> i) & 1);
                              return part() == i && accentEdit < 0 ? 1 : (int) ((eng.partFlash.load() >> i) & 1) * 2;
                          });
        if (i >= 9) b->text = juce::String (i - 8);
        b->textMm = 4.2f;
    }
    button (236.5f, 193, 12, 6, "ACCENT", [this] { accentEdit = accentEdit == 0 ? -1 : 0; message ("ACCENT", "DRUM STEPS"); }, [this] { return (int) (accentEdit == 0); });
    button (333, 193, 12, 6, "ACCENT", [this] { accentEdit = accentEdit == 1 ? -1 : 1; message ("ACCENT", "SYNTH STEPS"); }, [this] { return (int) (accentEdit == 1); });

    // ---- step section
    auto sh = button (92, 210, 12.6f, 12.6f, "SHIFT", [this] { shift = true; }, [this] { return (int) shift; });
    sh->round = true; sh->release = [this] { shift = false; };
    button (113.5f, 210, 13, 5, "PATTERN SET", [this] { message ("PATTERN SET", "not on XEMX"); });
    button (134.5f, 210, 13, 5, "KEYBOARD", [this] { keyboard = ! keyboard; }, [this] { return (int) keyboard; });
    button (158, 210, 12.5f, 8, "<", [this] { viewMeasure = juce::jmax (0, viewMeasure - 1); follow = false; });
    button (174, 210, 12.5f, 8, ">", [this] { viewMeasure = juce::jmin (pat().measures() - 1, viewMeasure + 1); follow = false; });
    for (int k = 0; k < 8; ++k)
    {
        led (189.0f + 10 * k, 206.7f, [this, k] { return eng.running.load() && eng.playMeasure.load() == k ? 2 : (int) (k < pat().measures() && ! eng.running.load() && false); });
        led (189.0f + 10 * k, 212.7f, [this, k] { return viewMeasure == k ? 1 : 0; });
    }
    for (int i = 0; i < 16; ++i)
    {
        auto* b = button (92.0f + 16 * i, 227, 13.5f, 15, juce::String (i + 1), [this, i] { stepPressed (i); }, [this, i]
        {
            auto p = pat();
            const int idx = viewMeasure * 16 + i;
            if (i >= p.lastStep()) return 0;
            const bool playing = eng.running.load() && eng.playStep.load() == i && eng.playMeasure.load() == viewMeasure;
            bool on;
            if (keyboard && synthSel()) on = false;
            else if (accentEdit >= 0) on = p.accentStep (accentEdit == 1, idx);
            else on = p.stepOn (part(), idx);
            return playing ? 2 : (int) on;
        });
        b->textMm = 4.4f; b->litColour = col::step;
    }
}

void XemxEditor::padPressed (int i)
{
    if (muteLatch) { eng.muteMask = eng.muteMask.load() ^ (1u << i); return; }
    if (soloLatch) { eng.soloMask = eng.soloMask.load() ^ (1u << i); return; }
    eng.selectedPart = i;
    accentEdit = -1;
    auto p = pat();
    if (! shift)
    {
        const int note = Pattern::isSynth (i) ? juce::jmax (36, p.note (i - 9, touchedStep) >= 0 ? p.note (i - 9, touchedStep) : 48) : 0;
        eng.audition (i, note);
        if (rec && eng.running.load() && ! Pattern::isSynth (i))
        {
            const int idx = eng.playMeasure.load() * 16 + juce::jmax (0, eng.playStep.load());
            if (! p.drumStep (i, idx)) p.toggleStep (i, idx, 0);
        }
    }
    const auto* b = p.part (i);
    message (partLabel (i), Pattern::isSynth (i) ? juce::String (emx::kOscTypes[b[2] & 15]) + " " + emx::waveNames (b[2] & 15)[b[0]] : juce::String (emx::kDrumWaves[juce::jmin (206, (int) b[0])]));
}

void XemxEditor::stepPressed (int i)
{
    auto p = pat();
    if (shift && i == 12) { openEmx(); return; }                               // SHIFT + 13 = CARD
    if (keyboard && synthSel())
    {
        const int note = juce::jlimit (0, 127, p.arpCentre() + i - 6);
        eng.audition (part(), note);
        if (rec && eng.running.load())
        {
            const int idx = eng.playMeasure.load() * 16 + juce::jmax (0, eng.playStep.load());
            auto* syb = p.synth (part() - 9); syb[20 + idx] = (uint8_t) note; if (syb[148 + idx] == 0) syb[148 + idx] = 2;
        }
        message ("KEYBOARD", noteName (note));
        return;
    }
    if (i >= p.lastStep()) return;
    const int idx = viewMeasure * 16 + i;
    touchedStep = idx;
    if (accentEdit >= 0) { p.toggleAccent (accentEdit == 1, idx); return; }
    int def = 48;
    if (synthSel()) for (int k = idx; k >= 0; --k) if (p.note (part() - 9, k) >= 0) { def = p.note (part() - 9, k); break; }
    p.toggleStep (part(), idx, def);
    if (synthSel() && p.note (part() - 9, idx) >= 0) message ("STEP " + juce::String (idx + 1), noteName (p.note (part() - 9, idx)));
}

juce::String XemxEditor::itemValue (int m, int it)
{
    auto p = pat();
    const auto* b = p.part (part());
    switch (m)
    {
        case modePattern:
            switch (it)
            {
                case 0: return juce::String::formatted ("%03d", eng.currentPattern.load() + 1);
                case 1: return juce::String (p.bpm(), 1);
                case 2: return num (p.swing()) + "%";
                case 3: return num (p.rolls());
                case 4: return num (p.measures());
                case 5: return kBeats[p.beat()];
                case 6: return emx::kScales[p.scale()];
                default: break;
            }
            break;
        case modePartEdit:
            switch (it)
            {
                case 0: return synthSel() ? emx::waveNames (b[2] & 15)[b[0]] : juce::String (emx::kDrumWaves[juce::jmin (206, (int) b[0])]);
                case 1: return synthSel() ? sgn (b[1] + 14) : "--";
                case 2: return p.outputLR (part()) ? "L/R" : "3/4";
                case 3: return p.swingOn (part()) ? "ON" : "OFF";
                case 4: return p.accentOn (part()) ? "ON" : "OFF";
                default: break;
            }
            break;
        case modeStepEdit:
            if (! synthSel()) return it == 4 ? "--" : "DRUM";
            if (it == 2) return juce::String (emx::gateSteps (p.gateByte (part() - 9, touchedStep)), 2);
            if (it == 3) return noteName (p.note (part() - 9, touchedStep));
            return "--";
        case modeSong:
        {
            auto songs = eng.file.songs();
            auto& sg = songs[(size_t) song];
            switch (it)
            {
                case 0: return juce::String::formatted ("S%02d ", song + 1) + sg.name;
                case 1: return juce::String (sg.bpm, 1);
                case 2: return num (eng.songPos.load() + 1) + "/" + num ((int) sg.patterns.size());
                case 3: return juce::String::formatted ("%03d", eng.currentPattern.load() + 1);
                default: return "--";
            }
        }
        case modeGlobal: return it == 1 ? "TAP" : "--";
        case modeMidi: return it == 0 ? "HOST" : it == 1 ? "ALL" : "2";
        default: break;
    }
    return {};
}

juce::String XemxEditor::itemName (int m, int it) const { return kItems[m][it]; }

void XemxEditor::jog (int d)
{
    auto p = pat();
    auto* b = p.part (part());
    switch (mode)
    {
        case modePattern:
            switch (item[mode])
            {
                case 0:
                {
                    const int next = juce::jlimit (0, 255, eng.currentPattern.load() + d);
                    eng.selectPattern (next);
                    if (! eng.running.load()) eng.currentPattern = next;
                    auto* prm = proc.params.getParameter ("pattern");
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next + 1));
                    return;
                }
                case 1: p.setBpm (p.bpm() + d * (shift ? 0.1 : 1.0)); return;
                case 2: p.p[0x0a] = (uint8_t) ((p.p[0x0a] & ~0x1f) | juce::jlimit (0, 25, (p.p[0x0a] & 0x1f) + d)); return;
                case 3: p.p[0x0b] = (uint8_t) ((p.p[0x0b] & 0x3f) | (juce::jlimit (0, 2, ((p.p[0x0b] >> 6) & 3) + d) << 6)); return;
                case 4: p.p[0x0b] = (uint8_t) ((p.p[0x0b] & ~7) | juce::jlimit (0, 7, (p.p[0x0b] & 7) + d)); return;
                case 5: p.p[0x0b] = (uint8_t) ((p.p[0x0b] & ~0x30) | (juce::jlimit (0, 3, ((p.p[0x0b] >> 4) & 3) + d) << 4)); return;
                case 6: p.p[0x0e] = (uint8_t) ((p.p[0x0e] & ~0x1f) | juce::jlimit (0, 30, (p.p[0x0e] & 0x1f) + d)); return;
                default: return;
            }
        case modePartEdit:
            switch (item[mode])
            {
                case 0:
                {
                    const int n = synthSel() ? emx::waveNames (b[2] & 15).size() : 207;
                    if (n > 0) b[0] = (uint8_t) juce::jlimit (0, n - 1, b[0] + d);
                    if (! synthSel()) eng.audition (part(), 0);
                    return;
                }
                case 1: if (synthSel()) b[1] = (uint8_t) juce::jlimit (0, 100, b[1] + d); return;
                case 2: p.setMask (0x14, part(), ! p.outputLR (part())); return;
                case 3: p.setMask (0x12, part(), ! p.swingOn (part())); return;
                case 4: p.setMask (0x16, part(), ! p.accentOn (part())); return;
                default: return;
            }
        case modeStepEdit:
            if (! synthSel()) return;
            if (item[mode] == 2) { auto& g = p.synth (part() - 9)[148 + touchedStep]; g = (uint8_t) juce::jlimit (0, 255, g + d); }
            if (item[mode] == 3) { auto& n = p.synth (part() - 9)[20 + touchedStep]; if (! (n & 0x80)) n = (uint8_t) juce::jlimit (0, 127, n + d); eng.audition (part(), n & 0x7f); }
            return;
        case modeSong:
            if (item[mode] == 0) { song = juce::jlimit (0, emx::kNumSongs - 1, song + d); eng.playSong (song); }
            return;
        default: return;
    }
}

void XemxEditor::openEmx()
{
    chooser = std::make_unique<juce::FileChooser> ("Load EMX-1 card file", juce::File(), "*.emx;*.EMX");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult(); if (! f.existsAsFile()) return;
        juce::String err;
        if (! proc.loadEmx (f, err)) message ("LOAD ERROR", err);
    });
}

void XemxEditor::saveEmx()
{
    chooser = std::make_unique<juce::FileChooser> ("Save EMX-1 card file", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (proc.emxName().endsWithIgnoreCase (".emx") ? proc.emxName() : "XEMX.EMX"), "*.emx;*.EMX");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult(); if (f == juce::File()) return;
        message ("WRITE", proc.saveEmx (f) ? "SAVED" : "FAILED");
    });
}

void XemxEditor::chooseSamples()
{
    chooser = std::make_unique<juce::FileChooser> ("EMX-1 sample folder or ZIP", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"), "*.zip");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.exists()) proc.loadSamplesAsync (f); });
}

void XemxEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension ("emx")) { juce::String err; if (! proc.loadEmx (f, err)) message ("LOAD ERROR", err); return; }
        if (f.hasFileExtension ("zip") || f.isDirectory()) { proc.loadSamplesAsync (f); message ("SAMPLES", "LOADING"); return; }
    }
}

void XemxEditor::timerCallback()
{
    if (follow && eng.running.load()) viewMeasure = juce::jmin (eng.playMeasure.load(), 7);
    for (auto* w : widgets) w->refresh();
    eng.partFlash = 0;
    // arpeggiator: retrigger the held note on 16ths of the current tempo
    if (arpNote >= 0 && synthSel())
    {
        const int every = juce::jmax (1, (int) std::round (60.0 / eng.displayBpm.load() / 4 * 30));
        if (arpCounter++ % every == 0) eng.audition (part(), arpNote);
    }
    // developer aid: XEMX_SNAPSHOT=<png path> writes the panel once
    static bool snapped = false;
    if (! snapped && ++snapTicks > 20)
    {
        snapped = true;
        const auto path = juce::SystemStats::getEnvironmentVariable ("XEMX_SNAPSHOT", {});
        if (path.isNotEmpty())
        {
            juce::FileOutputStream os { juce::File (path) };
            if (os.openedOk()) { os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (createComponentSnapshot (getLocalBounds(), true, 1.0f), os); }
        }
    }
    static int lastState = -1;
    const int st = proc.sampleState.load() * 1000 + eng.samples.count();
    if (st != lastState) { lastState = st; repaint (0, 0, getWidth(), (int) (14 * s)); }
}

void XemxEditor::resized()
{
    s = (float) getWidth() / kWidthMm;
    for (auto& p : placed)
    {
        p.w->s = s;
        p.w->setBounds (juce::Rectangle<float> (p.wMm * s, p.hMm * s).withCentre ({ p.cx * s, p.cy * s }).toNearestInt());
    }
    background = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (s));
    drawPanel (g);
}

void XemxEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    // live text in the top strip: card file and sample bank
    g.addTransform (juce::AffineTransform::scale (s));
    juce::String samples = proc.sampleState.load() == 1 ? "loading samples..." : eng.samples.count() > 0 ? juce::String (eng.samples.count()) + "/207 EMX waves" : "no samples (synth drums)";
    text (g, "CARD: " + proc.emxName(), 90, 5.5f, 60, 5, 2.4f, juce::Justification::centredLeft, col::dim, false);
    text (g, samples, 245, 5.5f, 100, 5, 2.4f, juce::Justification::centredRight, col::dim, false);
}

void XemxEditor::drawPanel (juce::Graphics& g)
{
    using J = juce::Justification;
    const float lw = 0.35f;
    // body
    g.setColour (juce::Colour (0xff111316)); g.fillRoundedRectangle (0, 0, kWidthMm, kHeightMm, 5);
    g.setColour (col::panel); g.fillRoundedRectangle (1.5f, 1.5f, kWidthMm - 3, kHeightMm - 3, 4.5f);
    g.setColour (col::line); g.drawRoundedRectangle (1.5f, 1.5f, kWidthMm - 3, kHeightMm - 3, 4.5f, lw);

    // screws
    for (auto [x, y] : std::initializer_list<std::pair<float, float>> { { 9.5f, 12.0f }, { 131.0f, 21.0f }, { 227.0f, 21.0f }, { 349.0f, 12.0f }, { 10.0f, 129.0f }, { 347.5f, 129.0f },
                         { 11.0f, 245.0f }, { 178.5f, 129.5f }, { 175.0f, 248.0f }, { 346.5f, 244.5f } })
    {
        g.setColour (col::knob); g.fillEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f);
        g.setColour (col::line); g.drawEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f, lw);
        juce::Path hex; hex.addPolygon ({ x, y }, 6, 1.5f); g.strokePath (hex, juce::PathStrokeType (lw));
    }

    // upper-left plate with the valve window cut-out
    juce::Path plate;
    plate.startNewSubPath (8.5f, 24);
    plate.quadraticTo (8.5f, 20, 12.5f, 20);
    plate.lineTo (120, 20);
    plate.cubicTo (135, 20, 140, 62, 179, 67);
    plate.cubicTo (218, 62, 223, 20, 236, 20);
    g.setColour (col::line); g.strokePath (plate, juce::PathStrokeType (lw));
    g.drawLine (8.5f, 24, 8.5f, 168.5f, lw);

    // logo (XEMX in place of KORG / ELECTRIBE MX)
    text (g, "XEMX", 17, 23, 30, 11, 10.5f, J::centredLeft, col::label, true);
    g.setColour (col::line); g.drawRect (49.0f, 24.5f, 54.0f, 8.0f, 0.6f);
    text (g, "E L E C T R O N I C", 49, 24.5f, 54, 8, 3.4f, J::centred, col::label, false);
    text (g, "MX", 104, 22.5f, 16, 12, 9.0f, J::centredLeft, col::accent, false);
    text (g, "MASTER VOLUME", 15, 38.5f, 29, 4, 2.2f);
    text (g, "TUBE GAIN", 44, 38.5f, 26, 4, 2.2f);
    text (g, "AUDIO IN LEVEL", 82, 38.5f, 31, 4, 2.2f);

    // valve window
    text (g, "VALVE FORCE", 160, 4.5f, 38, 5, 2.8f, J::centred, col::label);
    g.setColour (juce::Colour (0xff0a0b0d)); g.fillEllipse (144, 15, 70, 52);
    g.setColour (col::line); g.drawEllipse (144, 15, 70, 52, 0.6f);
    for (float tx : { 160.5f, 196.0f })
    {
        g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.55f), tx, 48, juce::Colours::transparentBlack, tx, 22, true));
        g.fillEllipse (tx - 10, 26, 20, 30);
        g.setColour (col::dim.withAlpha (0.8f));
        g.drawRoundedRectangle (tx - 8, 21, 16, 40, 6, 0.4f);
        g.drawLine (tx - 5, 32, tx + 5, 32, 0.4f); g.drawLine (tx - 5, 49, tx + 5, 49, 0.4f);
        g.drawLine (tx - 3, 34, tx - 3, 47, 0.5f); g.drawLine (tx + 3, 34, tx + 3, 47, 0.5f);
    }

    // LCD block
    g.setColour (col::line);
    g.drawRoundedRectangle (19.5f, 63, 145.5f, 46.5f, 3, lw);
    text (g, "XEMX-1", 31, 66.5f, 20, 6, 4.2f, J::centredLeft);
    text (g, "Music Production Station", 49, 67, 40, 5, 2.4f, J::centredLeft);
    text (g, "1 2 3 4 5 6A 6B 7A 7B", 32, 99.5f, 32, 4, 1.7f, J::centredLeft, col::dim, false);
    text (g, "1 2 3 4 5", 64.5f, 99.5f, 16, 4, 1.7f, J::centredLeft, col::dim, false);
    text (g, "DRUM PART", 33, 107, 30, 3, 1.7f, J::centred, col::dim);
    text (g, "SYNTH PART", 63, 107, 16, 3, 1.7f, J::centred, col::dim);
    text (g, "BEAT", 120, 75, 10, 3, 1.9f);
    text (g, "RENAME", 103, 96, 14, 3, 1.9f, J::centred, col::label, false);

    // mode item columns
    const float colX[6] { 36, 57, 78, 100, 122, 143 };
    for (int m = 0; m < 6; ++m)
    {
        g.setColour (col::line); g.drawLine (colX[m] - 1.2f, 112, colX[m] - 1.2f, m < 4 ? 133.5f : 127, lw);
        for (int i = 0; i < kItemCount[m]; ++i) text (g, kItems[m][i], colX[m], 111.8f + i * 3.0f, 21, 3, 1.9f, J::centredLeft);
    }
    text (g, "COMPARE", 38, 132.5f, 17, 3, 1.9f, J::centred, col::label, false);
    text (g, "ERASE", 18, 145.3f, 17, 3, 1.9f, J::centred, col::label, false);
    text (g, "REST/TIE", 146.5f, 146.6f, 11, 3, 1.7f);
    text (g, "TRANSPOSE", 162.5f, 146.6f, 12, 3, 1.7f);
    g.setColour (col::line);
    g.drawLine (8.5f, 145, 178.5f, 145, lw);
    g.drawLine (8.5f, 168.5f, 236, 168.5f, lw);
    g.drawLine (178.5f, 66, 178.5f, 168.5f, lw);
    g.drawLine (232.5f, 64, 232.5f, 168.5f, lw);

    // effect
    text (g, "EFFECT", 194, 64.5f, 25, 6, 3.6f);
    for (int i = 0; i < 16; ++i)
    {
        const float a = (float) (i - 8) * juce::MathConstants<float>::pi / 8, r = 16.0f, x = 206.4f + std::sin (a) * r, y = 89.3f - std::cos (a) * r;
        const auto j = std::sin (a) < -0.3f ? J::centredRight : std::sin (a) > 0.3f ? J::centredLeft : J::centred;
        const float bx = j == J::centredRight ? x - 24 : j == J::centredLeft ? x : x - 12;
        text (g, juce::String (emx::kFxTypes[i]).toUpperCase(), bx, y - 1.5f, 24, 3, 1.75f, j);
    }
    text (g, "FX EDIT 1", 182, 108.5f, 20, 4, 2.2f);
    text (g, "FX EDIT 2", 209.7f, 108.5f, 20, 4, 2.2f);
    for (int k = 0; k < 3; ++k)
    {
        const float y = 136.4f + k * 6.1f;
        g.setColour (col::line); g.drawRoundedRectangle (188.2f, y - 2, 9, 4, 0.8f, lw);
        text (g, "FX " + juce::String (k + 1), 188.2f, y - 2, 9, 4, 1.8f);
        text (g, "FX " + juce::String (k + 1), 213.5f, 130.5f + k * 6, 10, 3, 1.9f, J::centredLeft);
    }
    g.setColour (col::line);
    g.drawLine (181.5f, 136.4f, 188.2f, 136.4f, lw); g.drawLine (197.2f, 136.4f, 203.5f, 136.4f, lw);
    g.drawLine (203.5f, 136.4f, 203.5f, 148.5f, lw); g.drawLine (197.2f, 142.5f, 203.5f, 142.5f, lw); g.drawLine (197.2f, 148.6f, 203.5f, 148.6f, lw);
    g.drawLine (181.5f, 142.5f, 188.2f, 142.5f, lw); g.drawLine (181.5f, 148.6f, 188.2f, 148.6f, lw);

    // part common / modulation
    g.drawRoundedRectangle (236, 20, 112, 84.5f, 2.5f, lw);
    g.drawLine (288, 20, 288, 104.5f, lw);
    text (g, "PART COMMON", 240, 22.5f, 44, 7, 3.4f);
    text (g, "MODULATION", 292, 22.5f, 52, 7, 3.4f);
    text (g, "PITCH/GLIDE", 234, 33, 24, 4, 2.2f); text (g, "PAN", 262, 33, 24, 4, 2.2f);
    text (g, "EG TIME", 234, 56, 24, 4, 2.2f); text (g, "LEVEL", 262, 56, 24, 4, 2.2f);
    text (g, "L/3", 258.5f, 51, 8, 3, 1.8f); text (g, "R/4", 281, 51, 8, 3, 1.8f);
    text (g, "SPEED", 290, 33, 24, 4, 2.2f); text (g, "DEPTH", 318, 33, 24, 4, 2.2f);
    text (g, "-", 314, 51, 6, 3, 2.4f); text (g, "+", 336.5f, 51, 6, 3, 2.4f);
    for (int k = 0; k < 3; ++k) text (g, "FX" + juce::String (k + 1), 260.5f, 68 + k * 6, 8, 3, 1.9f, J::centredLeft);
    text (g, "EG", 239.6f, 74, 4, 3, 1.5f, J::centredLeft); text (g, "GT", 239.6f, 80, 4, 3, 1.5f, J::centredLeft);
    g.drawRoundedRectangle (277, 80.5f, 23.5f, 23.5f, 2, lw);
    text (g, "SMOOTH", 282.5f, 83.2f, 16, 3, 1.9f, J::centredLeft); text (g, "TRIG HOLD", 282.5f, 89.2f, 16, 3, 1.9f, J::centredLeft);
    const char* types[] { "SAW", "SQUARE", "TRIANGLE", "S&H", "EG" };
    const char* dests[] { "PITCH", "OSC EDIT 1", "OSC EDIT 2", "CUTOFF", "AMP", "PAN" };
    for (int k = 0; k < 5; ++k) text (g, types[k], 304.6f, 56 + k * 6, 16, 3, 1.8f, J::centredLeft);
    for (int k = 0; k < 6; ++k)
    {
        text (g, dests[k], 327.5f, 56 + k * 6, 18, 3, 1.8f, J::centredLeft);
        if (k >= 1 && k <= 3) { g.setColour (col::line); g.drawRoundedRectangle (323, 55.5f + k * 6, 22, 4, 0.8f, lw * 0.8f); }
    }

    // synth oscillator / filter
    g.setColour (col::line);
    g.drawRoundedRectangle (236, 104.5f, 112, 65, 2.5f, lw);
    g.drawLine (287, 104.5f, 287, 169.5f, lw);
    g.drawRoundedRectangle (237.5f, 105.8f, 47, 4.6f, 0.8f, lw);
    g.drawRoundedRectangle (289.5f, 105.8f, 57, 4.6f, 0.8f, lw);
    text (g, "SYNTH OSCILLATOR", 237.5f, 105.8f, 47, 4.6f, 2.6f);
    text (g, "SYNTH FILTER", 289.5f, 105.8f, 57, 4.6f, 2.6f);
    for (int i = 0; i < 16; ++i)
    {
        const float a = (float) (i - 7) * juce::MathConstants<float>::pi / 8, r = 15.8f, x = 260 + std::sin (a) * r, y = 131.5f - std::cos (a) * r;
        const auto j = std::sin (a) < -0.3f ? J::centredRight : std::sin (a) > 0.3f ? J::centredLeft : J::centred;
        const float bx = j == J::centredRight ? x - 22 : j == J::centredLeft ? x : x - 11;
        text (g, juce::String (emx::kOscTypes[i]).toUpperCase(), bx, y - 1.5f, 22, 3, 1.6f, j);
    }
    auto boxed = [&] (const char* t, float cx, float y, float w)
    {
        g.setColour (col::line); g.drawRoundedRectangle (cx - w / 2, y, w, 4, 0.8f, lw);
        text (g, t, cx - w / 2, y, w, 4, 2.0f);
    };
    boxed ("OSC EDIT 1", 246, 146.5f, 16); boxed ("OSC EDIT 2", 273.7f, 146.5f, 16);
    boxed ("CUTOFF", 302, 111.5f, 16); boxed ("RESONANCE", 330, 111.5f, 16);
    boxed ("EG INT", 302, 137.5f, 16); boxed ("DRIVE", 330, 137.5f, 16);
    const char* ft[] { "LPF", "HPF", "BPF", "BPF+" };
    for (int k = 0; k < 4; ++k) text (g, ft[k], (k % 2 ? 328.6f : 317.6f), (k < 2 ? 158.5f : 164.5f), 10, 3, 1.9f, J::centredLeft);

    // bottom section
    g.setColour (col::line);
    g.drawRoundedRectangle (8.5f, 170, 340, 79.5f, 2.5f, lw);
    g.drawLine (79, 170, 79, 249.5f, lw);
    g.drawLine (79, 200, 348.5f, 200, lw);
    g.drawLine (243.8f, 170, 243.8f, 200, lw);
    text (g, "ARPEGGIATOR", 26, 171, 40, 4.5f, 2.6f);
    text (g, "DRUM PART", 120, 170.5f, 64, 4, 2.6f);
    text (g, "SYNTH PART", 268, 170.5f, 64, 4, 2.6f);
    // arpeggiator control slider (decorative scale)
    for (int i = 0; i < 12; ++i) g.drawLine (51, 185 + i * 4.5f, 68, 185 + i * 4.5f, 0.25f);
    g.setColour (col::knob); g.fillRoundedRectangle (58, 183, 4, 53, 2);
    g.setColour (col::line); g.drawRoundedRectangle (58, 183, 4, 53, 2, lw);
    g.setColour (col::cap); g.fillRoundedRectangle (51.5f, 205.5f, 17, 7, 1);
    g.setColour (col::line); g.drawRoundedRectangle (51.5f, 205.5f, 17, 7, 1, lw); g.drawLine (52, 209, 68, 209, lw);

    text (g, "<<", 152, 202.5f, 12, 3, 1.8f, J::centred, col::dim, false);
    text (g, ">>", 168, 202.5f, 12, 3, 1.8f, J::centred, col::dim, false);
    for (int k = 0; k < 8; ++k) text (g, juce::String (k + 1), 186.0f + 10 * k, 201.6f, 6, 3, 1.9f);
    g.setColour (col::line);
    g.drawLine (85, 218.5f, 180, 218.5f, 0.6f); g.drawLine (186, 218.5f, 252, 218.5f, 0.6f); g.drawLine (259, 218.5f, 340, 218.5f, 0.6f);
    for (int i = 0; i < 16; ++i)
    {
        text (g, kStepLabels[i][0], 84.0f + 16 * i, 240.3f, 16, 2.6f, 1.7f, J::centred, col::label, false);
        text (g, kStepLabels[i][1], 84.0f + 16 * i, 242.7f, 16, 2.6f, 1.7f, J::centred, col::label, false);
    }
    g.setColour (col::line);
    g.drawLine (86, 238.5f, 210, 238.5f, lw); g.drawLine (215, 238.5f, 258, 238.5f, lw); g.drawLine (262, 238.5f, 290, 238.5f, lw);
    text (g, "PATTERN", 137, 236.6f, 22, 3, 1.9f, J::centred, col::label, false);
    text (g, "SONG", 228, 236.6f, 16, 3, 1.9f, J::centred, col::label, false);
    text (g, "GLOBAL/MIDI", 264, 236.6f, 24, 3, 1.9f, J::centred, col::label, false);
    text (g, "XEMX  -  EMX-1 pattern engine", 268, 247.5f, 70, 4, 2.0f, J::centredLeft, col::dim);
}
