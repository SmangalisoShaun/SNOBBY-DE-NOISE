#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SnobbyAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftSize = 2048;

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

    juce::AudioProcessorValueTreeState apvts;

    // values read by the editor
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f }, grDb { 0.f };
    std::atomic<float> specIn[24], specOut[24];   // dB, 4 points per band
    std::atomic<float> bandDb[6];                 // slow average band level in dB

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSample (std::array<float, fftSize>& fifo, int& pos, std::atomic<float>* dest, float x);
    void analyse (const std::array<float, fftSize>& fifo, std::atomic<float>* dest);

    juce::dsp::LinkwitzRileyFilter<float> xo[2][5];
    float env[2][6] = {};
    float avgL[6] = {};
    float avgCoef = 0.001f;
    double sr = 44100.0;

    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifoIn {}, fifoOut {};
    int posIn = 0, posOut = 0;
    std::array<float, 2 * fftSize> fftData {};
    float subEdge[25] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnobbyAudioProcessor)
};
