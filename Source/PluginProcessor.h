#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SnobbyAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftSize = 2048;                 // spectrum display
    static constexpr int dnN = 2048, dnHop = 512, dnBins = dnN / 2 + 1;   // denoiser

    SnobbyAudioProcessor();
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Snobby DeNoise"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void startLearn() { learnRequest.store (true); }

    juce::AudioProcessorValueTreeState apvts;

    // values read by the editor
    std::atomic<int>   learnState { 0 };                    // 0 idle, 1 learning, 2 finished
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f }, grDb { 0.f }, vuDb { -90.f };
    std::atomic<float> specIn[24], specOut[24], noiseDisp[24];

private:
    struct Chan
    {
        std::array<float, dnN> in {}, out {};
        std::array<float, 2 * dnN> fd {};
        std::array<float, dnBins> gS {}, gm {};
        int pos = 0, hopCount = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSample (std::array<float, fftSize>& fifo, int& pos, std::atomic<float>* dest,
                     std::atomic<float>* noise, float x);
    void analyse (const std::array<float, fftSize>& fifo, std::atomic<float>* dest, std::atomic<float>* noise);
    void processFrame (Chan& c, bool primary);

    double sr = 44100.0;

    // spectrum display
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifoIn {}, fifoOut {};
    int posIn = 0, posOut = 0;
    std::array<float, 2 * fftSize> fftData {};
    float subEdge[25] = {};

    // denoiser
    juce::dsp::FFT dnFft { 11 };
    float sqrtHann[dnN] = {};
    float fftScale = 1.f;
    Chan chan[2];
    std::array<float, dnBins> track {}, profile {}, acc {};
    std::array<int, dnBins> binBand {};
    bool haveProfile = false;
    int learnFrames = 0;
    std::atomic<bool> learnRequest { false };
    float cutGain[6] = { 1, 1, 1, 1, 1, 1 };
    float openCoef = 1.f, closeCoef = 0.1f, grFrame = 0.f;
    float vuMs = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnobbyAudioProcessor)
};
