#pragma once
#include "Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class XemxProcessor : public juce::AudioProcessor
{
public:
    XemxProcessor();
    ~XemxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "XEMX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // message thread helpers
    bool loadEmx (const juce::File&, juce::String& error);
    bool saveEmx (const juce::File&);
    void loadSamplesAsync (const juce::File&);
    juce::String emxName() const { return emxFile; }

    Engine engine;
    juce::AudioProcessorValueTreeState params;
    std::atomic<int> sampleState { 0 };   // 0 none, 1 loading, 2 ready
    std::function<void()> onFileChanged;  // set by the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioBuffer<float> inputCopy;
    juce::String emxFile = "Factory";
    juce::File emxPath;
    std::unique_ptr<juce::Thread> loader;
    juce::ThreadPool pool { 1 };
    int lastPatternParam = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XemxProcessor)
};
