#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static const char* bandIds[6]   = { "rumble", "body", "presence", "hissLow", "hissMid", "hissHigh" };
static const char* bandNames[6] = { "Rumble", "Body", "Presence", "Hiss Low", "Hiss Mid", "Hiss High" };
static const float bandEdges[7] = { 20.f, 120.f, 500.f, 5000.f, 8000.f, 12000.f, 20000.f };

SnobbyAudioProcessor::SnobbyAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& a : specIn)    a.store (-90.f);
    for (auto& a : specOut)   a.store (-90.f);
    for (auto& a : noiseDisp) a.store (-90.f);
}

AudioProcessorValueTreeState::ParameterLayout SnobbyAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const float maxCut[6] = { 10, 10, 10, 24, 36, 36 };

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

void SnobbyAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    for (int b = 0; b < 6; ++b)
        for (int s = 0; s < 4; ++s)
            subEdge[b * 4 + s] = bandEdges[b] * std::pow (bandEdges[b + 1] / bandEdges[b], (float) s / 4.f);
    subEdge[24] = 20000.f;
    fifoIn.fill (0.f); fifoOut.fill (0.f);
    posIn = posOut = 0;

    // denoiser setup
    for (int i = 0; i < dnN; ++i)
        sqrtHann[i] = std::sqrt (0.5f * (1.f - std::cos (MathConstants<float>::twoPi * (float) i / (float) dnN)));

    for (int k = 0; k < dnBins; ++k)
    {
        const float f = (float) k * (float) sampleRate / (float) dnN;
        binBand[(size_t) k] = f < 120.f ? 0 : f < 500.f ? 1 : f < 5000.f ? 2 : f < 8000.f ? 3 : f < 12000.f ? 4 : 5;
    }

    // measure the FFT round-trip gain so the output level is exactly right
    {
        std::array<float, 2 * dnN> t {}, ref {};
        for (int i = 0; i < dnN; ++i)
            t[(size_t) i] = ref[(size_t) i] = std::sin (0.05f * (float) i) + 0.5f * std::sin (0.31f * (float) i + 1.f);
        dnFft.performRealOnlyForwardTransform (t.data());
        dnFft.performRealOnlyInverseTransform (t.data());
        double num = 0.0, den = 0.0;
        for (int i = 0; i < dnN; ++i) { num += (double) t[(size_t) i] * ref[(size_t) i]; den += (double) ref[(size_t) i] * ref[(size_t) i]; }
        fftScale = (num > 0.0 && den > 0.0) ? (float) (den / num) : 1.f;
    }

    for (auto& c : chan) { c.in.fill (0.f); c.out.fill (0.f); c.gS.fill (1.f); c.gm.fill (1.f); c.pos = 0; c.hopCount = 0; }
    track.fill (0.f); profile.fill (0.f); acc.fill (0.f);
    haveProfile = false; learnFrames = 0; vuMs = 0.f;
    learnState.store (0);

    setLatencySamples (dnN);
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

void SnobbyAudioProcessor::analyse (const std::array<float, fftSize>& fifo, std::atomic<float>* dest, std::atomic<float>* noise)
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
        const float db = 10.f * std::log10 (p + 1.0e-12f);
        dest[k].store (db);
        if (noise != nullptr)
        {
            const float cur = noise[k].load();
            noise[k].store (db < cur ? cur + (db - cur) * 0.3f : cur + (db - cur) * 0.01f);
        }
    }
}

void SnobbyAudioProcessor::pushSample (std::array<float, fftSize>& fifo, int& pos, std::atomic<float>* dest,
                                       std::atomic<float>* noise, float x)
{
    fifo[(size_t) pos++] = x;
    if (pos >= fftSize) { pos = 0; analyse (fifo, dest, noise); }
}

void SnobbyAudioProcessor::processFrame (Chan& c, bool primary)
{
    for (int i = 0; i < dnN; ++i)
        c.fd[(size_t) i] = c.in[(size_t) ((c.pos + i) % dnN)] * sqrtHann[i];
    for (int i = dnN; i < 2 * dnN; ++i) c.fd[(size_t) i] = 0.f;

    dnFft.performRealOnlyForwardTransform (c.fd.data());

    const bool learning = primary && learnState.load() == 1;
    const float over = 2.5f;

    for (int k = 0; k < dnBins; ++k)
    {
        const float re = c.fd[(size_t) (2 * k)], im = c.fd[(size_t) (2 * k + 1)];
        const float m = std::sqrt (re * re + im * im);

        if (primary)
        {
            if (learning) acc[(size_t) k] += m;
            float& tr = track[(size_t) k];
            if (tr <= 0.f) tr = m;
            else if (m < tr) tr += (m - tr) * 0.4f;
            else tr += (m - tr) * 0.0008f;
        }

        const float nEst = haveProfile ? profile[(size_t) k] : track[(size_t) k] * 2.5f;
        const float r = over * nEst / (m + 1.0e-9f);
        float g = std::sqrt (jmax (0.f, 1.f - r * r));
        g = jmax (cutGain[binBand[(size_t) k]], g);

        float& gs = c.gS[(size_t) k];
        gs += (g - gs) * (g > gs ? openCoef : closeCoef);
    }

    float grSum = 0.f;
    for (int k = 0; k < dnBins; ++k)
    {
        const float a = c.gS[(size_t) jmax (0, k - 1)];
        const float b = c.gS[(size_t) k];
        const float d = c.gS[(size_t) jmin (dnBins - 1, k + 1)];
        const float v = 0.25f * a + 0.5f * b + 0.25f * d;
        c.gm[(size_t) k] = v;
        grSum -= 20.f * std::log10 (jmax (v, 1.0e-4f));
    }
    if (primary) grFrame = grSum / (float) dnBins;

    for (int j = 0; j < dnN; ++j)
    {
        const int kk = j <= dnN / 2 ? j : dnN - j;
        c.fd[(size_t) (2 * j)]     *= c.gm[(size_t) kk];
        c.fd[(size_t) (2 * j + 1)] *= c.gm[(size_t) kk];
    }

    dnFft.performRealOnlyInverseTransform (c.fd.data());

    const float scale = 0.5f * fftScale;
    for (int i = 0; i < dnN; ++i)
        c.out[(size_t) ((c.pos + i) % dnN)] += c.fd[(size_t) i] * sqrtHann[i] * scale;

    if (learning && ++learnFrames >= 200)
    {
        for (int k = 0; k < dnBins; ++k) profile[(size_t) k] = acc[(size_t) k] / (float) learnFrames;
        haveProfile = true;
        learnState.store (2);
    }
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
        for (int i = 0; i < n; ++i) pushSample (fifoIn, posIn, specIn, noiseDisp, 0.5f * (l[i] + r[i]));
    }

    if (learnRequest.exchange (false))
    {
        acc.fill (0.f);
        learnFrames = 0;
        learnState.store (1);
    }

    const bool byp = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    for (int b = 0; b < 6; ++b)
        cutGain[b] = byp ? 1.f : Decibels::decibelsToGain (apvts.getRawParameterValue (bandIds[b])->load());

    const float outG  = byp ? 1.f : Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());
    const float attMs = apvts.getRawParameterValue ("attack")->load();
    const float relMs = apvts.getRawParameterValue ("release")->load();
    const float hopSec = (float) dnHop / (float) sr;
    openCoef  = 1.f - std::exp (-hopSec / (0.001f * attMs));
    closeCoef = 1.f - std::exp (-hopSec / (0.001f * relMs));

    // streaming overlap-add: output is the input delayed by dnN samples
    for (int ch = 0; ch < nch; ++ch)
    {
        Chan& c = chan[ch];
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            c.in[(size_t) c.pos] = d[i];
            const float y = c.out[(size_t) c.pos];
            c.out[(size_t) c.pos] = 0.f;
            if (++c.pos >= dnN) c.pos = 0;
            d[i] = y * outG;
            if (++c.hopCount >= dnHop)
            {
                c.hopCount = 0;
                processFrame (c, ch == 0);
            }
        }
    }

    grDb.store (byp ? 0.f : grFrame);

    // VU level of the output, 300 ms style ballistics
    {
        float sq = 0.f;
        for (int ch = 0; ch < nch; ++ch)
        {
            const float* d = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i) sq += d[i] * d[i];
        }
        const float ms = sq / (float) (n * nch);
        vuMs += (ms - vuMs) * (1.f - std::exp (-(float) n / (0.065f * (float) sr)));
        vuDb.store (10.f * std::log10 (vuMs + 1.0e-12f));
    }

    {
        const float* l = buffer.getReadPointer (0);
        const float* r = nch > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i) pushSample (fifoOut, posOut, specOut, nullptr, 0.5f * (l[i] + r[i]));
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
