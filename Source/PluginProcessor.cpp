#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static const char* bandIds[6]   = { "rumble", "body", "presence", "hissLow", "hissMid", "hissHigh" };
static const char* bandNames[6] = { "Rumble", "Body", "Presence", "Hiss Low", "Hiss Mid", "Hiss High" };

SnobbyAudioProcessor::SnobbyAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()) {}

AudioProcessorValueTreeState::ParameterLayout SnobbyAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const float maxCut[6] = { 10, 10, 10, 26, 26, 26 };

    for (int i = 0; i < 6; ++i)
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { bandIds[i], 1 }, String (bandNames[i]) + " cut",
            NormalisableRange<float> (-maxCut[i], 0.f, 0.1f), 0.f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack (ms)",
        NormalisableRange<float> (0.1f, 100.f, 0.01f, 0.4f), 10.f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release (ms)",
        NormalisableRange<float> (10.f, 1000.f, 0.1f, 0.4f), 100.f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output (dB)",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

void SnobbyAudioProcessor::prepareToPlay (double, int) {}

bool SnobbyAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

static void raiseTo (std::atomic<float>& a, float v)
{
    float c = a.load();
    while (v > c && ! a.compare_exchange_weak (c, v)) {}
}

void SnobbyAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const float inPk = buffer.getMagnitude (0, n);
    raiseTo (inLevel, inPk);

    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f)
    {
        raiseTo (outLevel, inPk);
        return;
    }

    // placeholder: output gain only. Band splitting and cleaning go here next.
    buffer.applyGain (Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    raiseTo (outLevel, buffer.getMagnitude (0, n));
}

AudioProcessorEditor* SnobbyAudioProcessor::createEditor() { return new SnobbyEditor (*this); }

void SnobbyAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SnobbyAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnobbyAudioProcessor(); }
