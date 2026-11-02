#include "SampleBank.h"
#include "EmxData.h"

// Archive.org names ("BassDrum-88-1.wav") and EMX wave names ("BD-88 1") normalise to the same key.
juce::String SampleBank::key (juce::String s)
{
    s = s.toLowerCase().fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false);
    if (s.endsWith (".wav")) s = s.dropLastCharacters (4);
    s = s.replace ("bassdrum", "bd").replace ("lynn", "lyn").replaceFirstOccurrenceOf ("crck1", "crckl").replaceFirstOccurrenceOf ("noise", "nois");
    return s.retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789&");
}

int SampleBank::load (const juce::File& source)
{
    std::map<juce::String, int> index;
    for (int i = 0; i < 207; ++i) index[key (emx::kDrumWaves[i])] = i;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto next = std::make_shared<Set>();
    int n = 0;

    auto take = [&] (const juce::String& name, std::unique_ptr<juce::InputStream> in)
    {
        auto it = index.find (key (name));
        if (it == index.end() || ! in || (*next)[(size_t) it->second]) return;
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (std::move (in)));
        if (! r || r->lengthInSamples <= 0) return;
        auto s = std::make_shared<DrumSample>();
        const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, 44100 * 12);
        juce::AudioBuffer<float> tmp ((int) r->numChannels, len);
        r->read (&tmp, 0, len, 0, true, true);
        s->audio.setSize (1, len);
        s->audio.clear();
        for (int c = 0; c < tmp.getNumChannels(); ++c) s->audio.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) tmp.getNumChannels());
        s->rate = r->sampleRate;
        (*next)[(size_t) it->second] = s;
        ++n;
    };

    if (source.hasFileExtension ("zip"))
    {
        juce::ZipFile zip (source);
        for (int i = 0; i < zip.getNumEntries(); ++i)
            if (auto* e = zip.getEntry (i); e && e->filename.endsWithIgnoreCase (".wav"))
                take (e->filename, std::unique_ptr<juce::InputStream> (zip.createStreamForEntry (i)));
    }
    else if (source.isDirectory())
    {
        for (auto& f : source.findChildFiles (juce::File::findFiles, true, "*.wav"))
            take (f.getFileName(), f.createInputStream());
    }
    if (n == 0) return 0;
    std::atomic_store (&set, std::shared_ptr<const Set> (next));
    loaded = n;
    from = source;
    return n;
}
