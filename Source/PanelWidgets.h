#pragma once
// Panel controls drawn in the XEMX colour scheme. Every control is placed in millimetres
// (the EMX-1 panel is 358 x 256 mm, positions measured from the service manual drawing).
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace ui
{
namespace col
{
const juce::Colour panel { 0xff1c1f24 }, panelHi { 0xff262a31 }, line { 0xff6f7782 }, label { 0xffd8dde4 }, dim { 0xff8c939e };
const juce::Colour accent { 0xffff5a1f }, led { 0xffff3b1f }, ledOff { 0xff3a1d18 }, knob { 0xff0d0e10 }, cap { 0xff2b2f36 };
const juce::Colour lcdBg { 0xff9fd33a }, lcdInk { 0xff16200a }, button { 0xff30343c }, buttonHi { 0xff3c414b }, step { 0xffffa21f };
}

inline juce::Font font (float mm, bool bold = true) { return juce::Font (juce::FontOptions (mm, bold ? juce::Font::bold : juce::Font::plain)); }

// Base: knows its scale (px per mm) and can refresh itself when its visible state changes.
class Widget : public juce::Component
{
public:
    float s = 3.0f;
    std::function<juce::int64()> state;           // anything that changes the drawing
    void refresh() { if (state) { auto v = state(); if (v != last) { last = v; repaint(); } } }
private:
    juce::int64 last = -1;
};

class Knob : public Widget
{
public:
    std::function<int()> get;
    std::function<void (int)> set;
    std::function<bool()> enabled = [] { return true; };
    std::function<void()> touched;
    int minV = 0, maxV = 127, positions = 0;     // positions > 0: detented selector
    bool bipolar = false;
    float startAngle = -2.4f, endAngle = 2.4f;    // radians from 12 o'clock
    float ringMm = 7.0f;

    Knob() { state = [this] { return (juce::int64) (get ? get() : 0) * 2 + (enabled() ? 1 : 0); }; }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float r = ringMm * s, on = enabled() ? 1.0f : 0.35f;
        // tick ring
        g.setColour (col::line.withMultipliedAlpha (on));
        const int ticks = positions > 0 ? positions : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f, c.x + std::sin (a) * r, c.y - std::cos (a) * r, 0.3f * s);
        }
        if (bipolar) { juce::Path t; t.addTriangle (c.x - 0.9f * s, c.y - r - 1.4f * s, c.x + 0.9f * s, c.y - r - 1.4f * s, c.x, c.y - r - 0.2f * s); g.fillPath (t); }
        const float kr = r * 0.74f;
        g.setColour (col::knob);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (col::line.withMultipliedAlpha (0.8f * on));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 0.35f * s);
        const float cr = kr * 0.78f;
        g.setGradientFill (juce::ColourGradient (col::cap.brighter (0.25f), c.x, c.y - cr, col::cap.darker (0.4f), c.x, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        const float v = get ? (float) (get() - minV) / (float) juce::jmax (1, maxV - minV) : 0.0f;
        const float a = startAngle + (endAngle - startAngle) * v;
        g.setColour (col::label.withMultipliedAlpha (on));
        g.drawLine (c.x + std::sin (a) * cr * 0.25f, c.y - std::cos (a) * cr * 0.25f, c.x + std::sin (a) * cr * 0.95f, c.y - std::cos (a) * cr * 0.95f, 0.7f * s);
    }
    void mouseDown (const juce::MouseEvent&) override { if (enabled() && get) { dragStart = get(); acc = 0; if (touched) touched(); } }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! enabled() || ! set) return;
        const float range = positions > 0 ? (float) positions * 6.0f : 160.0f;  // px for full travel
        const float dy = (float) -e.getDistanceFromDragStartY() + (float) e.getDistanceFromDragStartX() * 0.3f;
        const int v = juce::jlimit (minV, maxV, dragStart + (int) std::round (dy / range * (float) (maxV - minV) * (e.mods.isShiftDown() ? 0.2f : 1.0f)));
        if (v != get()) set (v);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! enabled() || ! set || ! get) return;
        acc += w.deltaY * 20.0f;
        const int d = (int) acc; if (d == 0) return; acc -= (float) d;
        set (juce::jlimit (minV, maxV, get() + d));
        if (touched) touched();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (enabled() && set && bipolar) set ((minV + maxV + 1) / 2); }

private:
    int dragStart = 0;
    float acc = 0;
};

// Endless jog dial: reports steps.
class Jog : public Widget
{
public:
    std::function<void (int)> turned;
    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float r = getWidth() * 0.5f - 0.5f * s;
        g.setColour (col::knob); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (col::line); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.4f * s);
        for (int i = 0; i < 16; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 16;
            juce::Path p;
            p.addEllipse (-0.9f * s, -2.2f * s, 1.8f * s, 4.4f * s);
            p.applyTransform (juce::AffineTransform::rotation (a).translated (c.x + std::sin (a) * r * 0.72f, c.y - std::cos (a) * r * 0.72f));
            g.setColour (col::cap.brighter (0.2f)); g.fillPath (p);
        }
        const float cr = r * 0.45f;
        g.setColour (col::cap); g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    }
    void mouseDown (const juce::MouseEvent& e) override { lastY = (float) e.y; lastX = (float) e.x; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        acc += ((float) e.x - lastX) * 0.5f - ((float) e.y - lastY); lastX = (float) e.x; lastY = (float) e.y;
        step();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { acc += w.deltaY * 60.0f; step(); }

private:
    void step()
    {
        const float unit = 3.0f * s;
        while (std::abs (acc) >= unit) { const int d = acc > 0 ? 1 : -1; acc -= (float) d * unit; angle += 0.2f * (float) d; if (turned) turned (d); }
        repaint();
    }
    float acc = 0, angle = 0, lastX = 0, lastY = 0;
};

// Rounded panel button with an optional built-in light.
class Button : public Widget
{
public:
    juce::String text;
    std::function<void()> click;
    std::function<void()> release;
    std::function<int()> lit = [] { return 0; };  // 0 off, 1 on, 2 bright (playhead)
    juce::Colour litColour = col::led;
    float textMm = 2.0f, corner = 1.6f;
    bool big = false, round = false, italicNumber = false;
    juce::String drawing;                          // transport glyphs: "rew" "rec" "stop" "play"

    Button() { state = [this] { return (juce::int64) lit() * 2 + (pressed ? 1 : 0); }; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.3f * s);
        const int l = lit();
        const juce::Colour face = pressed ? col::buttonHi : col::button;
        g.setColour (col::knob);
        if (round) g.fillEllipse (b); else g.fillRoundedRectangle (b, corner * s);
        auto inner = b.reduced (0.6f * s);
        g.setColour (l ? (l == 2 ? litColour.brighter (0.6f) : litColour).withAlpha (big ? 0.85f : 0.9f) : face);
        if (round) g.fillEllipse (inner); else g.fillRoundedRectangle (inner, corner * 0.8f * s);
        g.setColour (col::line);
        if (round) g.drawEllipse (b, 0.3f * s); else g.drawRoundedRectangle (b, corner * s, 0.3f * s);
        g.setColour (l ? col::knob : col::label);
        if (drawing.isNotEmpty()) { glyph (g, inner); return; }
        g.setFont (font (textMm * s).italicised());
        g.drawFittedText (text, inner.toNearestInt().reduced ((int) (0.4f * s)), juce::Justification::centred, 2, 0.8f);
    }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); if (click) click(); }
    void mouseUp (const juce::MouseEvent&) override { pressed = false; repaint(); if (release) release(); }

private:
    void glyph (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto c = r.getCentre(); const float u = s;
        g.setColour (lit() ? col::knob : col::label);
        juce::Path p;
        if (drawing == "rew") { g.fillRect (c.x - 3.5f * u, c.y - 3 * u, 0.9f * u, 6 * u); p.addTriangle (c.x + 3.5f * u, c.y - 3 * u, c.x + 3.5f * u, c.y + 3 * u, c.x - 2.2f * u, c.y); g.strokePath (p, juce::PathStrokeType (0.6f * u)); }
        else if (drawing == "rec") g.drawEllipse (c.x - 3 * u, c.y - 3 * u, 6 * u, 6 * u, 0.6f * u);
        else if (drawing == "stop") g.drawRect (c.x - 3 * u, c.y - 3 * u, 6 * u, 6 * u, 0.6f * u);
        else if (drawing == "play")
        {
            p.addTriangle (c.x - 8 * u, c.y - 3 * u, c.x - 8 * u, c.y + 3 * u, c.x - 2.5f * u, c.y);
            g.strokePath (p, juce::PathStrokeType (0.6f * u));
            g.drawLine (c.x - 1 * u, c.y + 3 * u, c.x + 1.8f * u, c.y - 3 * u, 0.6f * u);
            g.drawRect (c.x + 3 * u, c.y - 3 * u, 1.4f * u, 6 * u, 0.6f * u);
            g.drawRect (c.x + 6 * u, c.y - 3 * u, 1.4f * u, 6 * u, 0.6f * u);
        }
    }
    bool pressed = false;
};

class Led : public Widget
{
public:
    std::function<int()> lit = [] { return 0; };
    Led() { state = [this] { return (juce::int64) lit(); }; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.15f * s);
        const int l = lit();
        if (l) { g.setColour (col::led.withAlpha (0.35f)); g.fillEllipse (b.expanded (0.0f)); }
        g.setColour (l ? col::led.brighter (l == 2 ? 0.6f : 0.0f) : col::ledOff);
        g.fillEllipse (b.reduced (0.25f * s));
        g.setColour (col::line); g.drawEllipse (b.reduced (0.25f * s), 0.2f * s);
    }
};
} // namespace ui
