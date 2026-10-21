#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static const char* bandIds[6]   = { "rumble", "body", "presence", "hissLow", "hissMid", "hissHigh" };
static const char* bandNames[6] = { "Rumble", "Body", "Presence", "Hiss Low", "Hiss Mid", "Hiss High" };
static const char* thrIds[6]    = { "thr0", "thr1", "thr2", "thr3", "thr4", "thr5" };
static const float xoverHz[5]   = { 120.f, 500.f, 5000.f, 8000.f, 12000.f };
static const float bandEdges[7] = { 20.f, 120.f, 500.f, 5000.f, 8000.f, 12000.f, 20000.f };

SnobbyAudioProcessor::SnobbyAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& a : specIn)  a.store (-90.f);
    for (auto& a : specOut) a.store (-90.f);
    for (auto& a : bandDb)  a.store (-90.f);
}

AudioProcessorValueTreeState::ParameterLayout SnobbyAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const float maxCut[6] = { 10, 10, 10, 26, 26, 26 };
    const float thrDef[6] = { -50, -50, -50, -58, -58, -58 };

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

    for (int i = 0; i < 6; ++i)
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { thrIds[i], 1 }, "Threshold " + String (i + 1),
            NormalisableRange<float> (-90.f, -20.f, 0.1f), thrDef[i]));

    return { p.begin(), p.end() };
}

void SnobbyAudioProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) blockSize, 1 };
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < 5; ++k)
        {
            xo[ch][k].prepare (spec);
            xo[ch][k].setCutoffFrequency (xoverHz[k]);
            xo[ch][k].reset();
        }

    for (auto& row : env) for (auto& v : row) v = 0.f;
    for (auto& v : avgL) v = 0.f;
    avgCoef = 1.f - std::exp (-1.f / (0.05f * (float) sampleRate));

    for (int b = 0; b < 6; ++b)
        for (int s = 0; s < 4; ++s)
            subEdge[b * 4 + s] = bandEdges[b] * std::pow (bandEdges[b + 1] / bandEdges[b], (float) s / 4.f);
    subEdge[24] = 20000.f;

    fifoIn.fill (0.f); fifoOut.fill (0.f);
    posIn = posOut = 0;
}

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

void SnobbyAudioProcessor::analyse (const std::array<float, fftSize>& fifo, std::atomic<float>* dest)
{
    std::copy (fifo.begin(), fifo.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binHz = (float) sr / (float) fftSize;
    for (int k = 0; k < 24; ++k)
    {
        const int lo = jmax (1, (int) std::floor (subEdge[k] / binHz));
        int hi = jmin (fftSize / 2 - 1, (int) std::floor (subEdge[k + 1] / binHz));
        if (hi < lo) hi = lo;
        float p = 0.f;
        for (int b = lo; b <= hi; ++b)
        {
            const float m = fftData[(size_t) b] / ((float) fftSize * 0.25f);
            p += m * m;
        }
        dest[k].store (10.f * std::log10 (p + 1.0e-12f));
    }
}

void SnobbyAudioProcessor::pushSample (std::array<float, fftSize>& fifo, int& pos, std::atomic<float>* dest, float x)
{
    fifo[(size_t) pos++] = x;
    if (pos >= fftSize) { pos = 0; analyse (fifo, dest); }
}

void SnobbyAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = jmin (2, buffer.getNumChannels());
    if (n == 0 || nch == 0) return;

    raiseTo (inLevel, buffer.getMagnitude (0, n));

    {
        const float* l = buffer.getReadPointer (0);
        const float* r = nch > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i) pushSample (fifoIn, posIn, specIn, 0.5f * (l[i] + r[i]));
    }

    const bool byp = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (byp)
    {
        grDb.store (0.f);
    }
    else
    {
        float cut[6], thr[6];
        for (int b = 0; b < 6; ++b)
        {
            cut[b] = apvts.getRawParameterValue (bandIds[b])->load();
            thr[b] = apvts.getRawParameterValue (thrIds[b])->load();
        }
        const float attMs = apvts.getRawParameterValue ("attack")->load();
        const float relMs = apvts.getRawParameterValue ("release")->load();
        const float outG  = Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());
        const float aA = 1.f - std::exp (-1.f / (0.001f * attMs * (float) sr));
        const float aR = 1.f - std::exp (-1.f / (0.001f * relMs * (float) sr));
        float grSum = 0.f;

        for (int ch = 0; ch < nch; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float rest = d[i];
                float bnd[6];
                for (int k = 0; k < 5; ++k)
                {
                    float lo = 0.f, hi = 0.f;
                    xo[ch][k].processSample (0, rest, lo, hi);
                    bnd[k] = lo;
                    rest = hi;
                }
                bnd[5] = rest;

                float sum = 0.f;
                for (int b = 0; b < 6; ++b)
                {
                    const float a = std::abs (bnd[b]);
                    float& e = env[ch][b];
                    e += (a > e ? aA : aR) * (a - e);
                    const float eDb = Decibels::gainToDecibels (e, -120.f);
                    const float t = jlimit (0.f, 1.f, (thr[b] - eDb) / 6.f);
                    const float gdb = cut[b] * t;
                    sum += bnd[b] * Decibels::decibelsToGain (gdb);
                    if (ch == 0)
                    {
                        avgL[b] += (a - avgL[b]) * avgCoef;
                        grSum -= gdb;
                    }
                }
                d[i] = sum * outG;
            }
        }

        grDb.store (grSum / (float) (n * 6) * 1.2f);
        for (int b = 0; b < 6; ++b) bandDb[b].store (Decibels::gainToDecibels (avgL[b], -120.f));
    }

    if (byp)
    {
        // keep the band meters alive while bypassed
        const float* d = buffer.getReadPointer (0);
        for (int i = 0; i < n; ++i) (void) d[i];
    }

    {
        const float* l = buffer.getReadPointer (0);
        const float* r = nch > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i) pushSample (fifoOut, posOut, specOut, 0.5f * (l[i] + r[i]));
    }
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
