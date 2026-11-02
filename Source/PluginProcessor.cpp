#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout XemxProcessor::layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master Volume", NormalisableRange<float> (0, 127, 1), 100.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tube", 1 }, "Tube Gain", NormalisableRange<float> (0, 127, 1), 40.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "audioIn", 1 }, "Audio In Level", NormalisableRange<float> (0, 127, 1), 100.0f));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "pattern", 1 }, "Pattern", 1, 256, 1));
    return l;
}

XemxProcessor::XemxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Audio In", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "XEMX", layout())
{
    // Default sample bank: the user's EMX-1 folder or ZIP in Downloads/ELECTRIBE, if present.
    auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads").getChildFile ("ELECTRIBE");
    for (auto& f : dir.findChildFiles (juce::File::findFilesAndDirectories, false, "*EMX*"))
        if (f.isDirectory() || f.hasFileExtension ("zip")) { loadSamplesAsync (f); break; }
}

XemxProcessor::~XemxProcessor() { pool.removeAllJobs (true, 5000); }

bool XemxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void XemxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 4096));
    inputCopy.setSize (2, juce::jmax (samplesPerBlock, 4096));
}

void XemxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n > inputCopy.getNumSamples()) { buffer.clear(); return; }

    const int inCh = getTotalNumInputChannels();
    const juce::AudioBuffer<float>* in = nullptr;
    if (inCh > 0)
    {
        inputCopy.setSize (juce::jmin (2, inCh), n, false, false, true);
        for (int c = 0; c < inputCopy.getNumChannels(); ++c) inputCopy.copyFrom (c, 0, buffer, c, 0, n);
        in = &inputCopy;
    }

    engine.masterVolume = params.getRawParameterValue ("master")->load();
    engine.tubeGain = params.getRawParameterValue ("tube")->load();
    engine.audioInLevel = params.getRawParameterValue ("audioIn")->load();
    const int pat = (int) params.getRawParameterValue ("pattern")->load() - 1;
    if (pat != lastPatternParam) { if (lastPatternParam >= 0) engine.selectPattern (pat); lastPatternParam = pat; }

    Engine::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.hostPlaying = pos->getIsPlaying();
            t.ppq = pos->getPpqPosition().orFallback (0.0);
            t.bpm = pos->getBpm().orFallback (120.0);
        }
    engine.process (buffer, in, midi, t);
}

bool XemxProcessor::loadEmx (const juce::File& f, juce::String& error)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { error = "Could not read " + f.getFileName(); return false; }
    {
        const juce::ScopedLock l (engine.fileLock);
        if (! engine.file.load (mb.getData(), mb.getSize(), error)) return false;
    }
    emxFile = f.getFileName(); emxPath = f;
    engine.selectPattern (engine.currentPattern.load());
    if (onFileChanged) onFileChanged();
    return true;
}

bool XemxProcessor::saveEmx (const juce::File& f)
{
    const juce::ScopedLock l (engine.fileLock);
    if (! f.replaceWithData (engine.file.bytes().getData(), engine.file.bytes().getSize())) return false;
    emxFile = f.getFileName(); emxPath = f;
    return true;
}

void XemxProcessor::loadSamplesAsync (const juce::File& src)
{
    sampleState = 1;
    pool.addJob ([this, src]
    {
        const int n = engine.samples.load (src);
        sampleState = n > 0 ? 2 : 0;
    });
}

void XemxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("emxName", emxFile);
    xml->setAttribute ("emxPath", emxPath.getFullPathName());
    xml->setAttribute ("samples", engine.samples.source().getFullPathName());
    xml->setAttribute ("part", engine.selectedPart.load());
    xml->setAttribute ("currentPattern", engine.currentPattern.load());
    juce::MemoryOutputStream z;
    {
        juce::GZIPCompressorOutputStream gz (z, 6);
        const juce::ScopedLock l (engine.fileLock);
        gz.write (engine.file.bytes().getData(), engine.file.bytes().getSize());
    }
    xml->setAttribute ("emx", z.getMemoryBlock().toBase64Encoding());
    copyXmlToBinary (*xml, dest);
}

void XemxProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (! xml) return;
    juce::MemoryBlock packed;
    if (packed.fromBase64Encoding (xml->getStringAttribute ("emx")))
    {
        juce::MemoryInputStream mi (packed, false);
        juce::GZIPDecompressorInputStream gz (mi);
        juce::MemoryBlock raw;
        gz.readIntoMemoryBlock (raw);
        juce::String err;
        const juce::ScopedLock l (engine.fileLock);
        engine.file.load (raw.getData(), raw.getSize(), err);
    }
    emxFile = xml->getStringAttribute ("emxName", "Factory");
    emxPath = juce::File (xml->getStringAttribute ("emxPath"));
    engine.selectedPart = xml->getIntAttribute ("part", 0);
    engine.selectPattern (xml->getIntAttribute ("currentPattern", 0));
    const juce::File samples (xml->getStringAttribute ("samples"));
    if (samples.exists() && samples != engine.samples.source()) loadSamplesAsync (samples);
    xml->removeAttribute ("emx");
    if (xml->hasTagName (params.state.getType())) params.replaceState (juce::ValueTree::fromXml (*xml));
    lastPatternParam = (int) params.getRawParameterValue ("pattern")->load() - 1;
    if (onFileChanged) onFileChanged();
}

juce::AudioProcessorEditor* XemxProcessor::createEditor() { return new XemxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XemxProcessor(); }
