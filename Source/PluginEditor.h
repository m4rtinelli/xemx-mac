#pragma once
#include "PluginProcessor.h"
#include "PanelWidgets.h"

class XemxEditor : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit XemxEditor (XemxProcessor&);
    ~XemxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr float kWidthMm = 358.0f, kHeightMm = 256.0f;

private:
    enum Mode { modePattern, modePartEdit, modeStepEdit, modeSong, modeGlobal, modeMidi };
    struct Placed { ui::Widget* w; float cx, cy, wMm, hMm; };

    void timerCallback() override;
    void build();
    void drawPanel (juce::Graphics&);   // static artwork, in millimetres

    template <class T> T* add (std::unique_ptr<T> w, float cx, float cy, float wMm, float hMm)
    {
        auto* raw = w.get();
        addAndMakeVisible (*raw);
        placed.push_back ({ raw, cx, cy, wMm, hMm });
        owned.push_back (std::move (w));
        return raw;
    }
    ui::Knob* knob (float cx, float cy, float ringMm, juce::String name, std::function<int()> get, std::function<void (int)> set,
                    std::function<juce::String (int)> fmt, std::function<bool()> enabled = {}, bool bipolar = false);
    ui::Button* button (float cx, float cy, float w, float h, juce::String text, std::function<void()> click, std::function<int()> lit = {});
    ui::Led* led (float cx, float cy, std::function<int()> lit, float d = 2.4f);

    // model access (current pattern, selected part)
    emx::Pattern pat() { return eng.file.pattern (eng.currentPattern.load()); }
    int part() const { return eng.selectedPart.load(); }
    bool synthSel() const { return emx::Pattern::isSynth (part()); }
    uint8_t* partBytes() { return pat().part (part()); }
    uint8_t& partByte (int drumOff, int synthOff) { return partBytes()[synthSel() ? synthOff : drumOff]; }
    void message (const juce::String& a, const juce::String& b);
    juce::String itemName (int mode, int item) const;
    juce::String itemValue (int mode, int item);
    void jog (int delta);
    void stepPressed (int i);
    void padPressed (int part);
    void openEmx();
    void saveEmx();
    void chooseSamples();
    juce::String partLabel (int p) const;

    XemxProcessor& proc;
    Engine& eng;
    float s = 3.2f;
    juce::Image background;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Placed> placed;
    std::vector<ui::Widget*> widgets;

    int mode = modePattern, item[6] {}, viewMeasure = 0, fxSlot = 0, accentEdit = -1, touchedStep = 0, song = 0;
    bool shift = false, keyboard = false, muteLatch = false, soloLatch = false, rec = false, follow = true;
    juce::String msgA, msgB;
    juce::uint32 msgUntil = 0;
    juce::Array<double> taps;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComponentBoundsConstrainer constrainer;
    int arpNote = -1, arpCounter = 0, snapTicks = 0;

    friend class Lcd;
    friend class TouchPad;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XemxEditor)
};
