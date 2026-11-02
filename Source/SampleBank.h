#pragma once
// The 207 EMX-1 drum waves, loaded from the user's own sample folder or ZIP (not shipped with the plugin).
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>

struct DrumSample
{
    juce::AudioBuffer<float> audio; // mono
    double rate = 44100;
};

class SampleBank
{
public:
    using Set = std::array<std::shared_ptr<const DrumSample>, 207>;

    // Loads a .zip or a folder (searched recursively). Returns how many waves matched.
    int load (const juce::File& source);
    std::shared_ptr<const Set> current() const { return std::atomic_load (&set); }
    int count() const { return loaded; }
    juce::File source() const { return from; }

    static juce::String key (juce::String name);

private:
    std::shared_ptr<const Set> set = std::make_shared<Set>();
    int loaded = 0;
    juce::File from;
};
