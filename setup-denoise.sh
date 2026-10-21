#!/usr/bin/env bash
set -e
cd /workspaces/SNOBBY-DE-NOISE
if [ "$(git rev-parse --show-toplevel)" != "$PWD" ]; then echo "Not the repo root, stopping."; exit 1; fi

# clear out the earlier attempts
rm -rf snobby-denoise build CMakeLists.txt Source .github

# JUCE as a submodule at the repo root, same as the delay plugin
if [ ! -f JUCE/CMakeLists.txt ]; then
  git submodule add --depth 1 https://github.com/juce-framework/JUCE.git JUCE \
    || git submodule add https://github.com/juce-framework/JUCE.git JUCE
fi

mkdir -p Source .github/workflows

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.22)
project(SnobbyDeNoise VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_plugin(SnobbyDeNoise
    COMPANY_NAME "BoomHype Tech"
    PLUGIN_MANUFACTURER_CODE Bhyp
    PLUGIN_CODE Sdn1
    FORMATS VST3
    PRODUCT_NAME "Snobby DeNoise"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx)

target_sources(SnobbyDeNoise PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SnobbyDeNoise PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SnobbyDeNoise PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
EOF

cat > Source/PluginProcessor.h << 'EOF'
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SnobbyAudioProcessor : public juce::AudioProcessor
{
public:
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnobbyAudioProcessor)
};
EOF

cat > Source/PluginProcessor.cpp << 'EOF'
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

void SnobbyAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f) return;

    // placeholder: output gain only. Band splitting and cleaning go here next.
    buffer.applyGain (Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
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
EOF

cat > Source/PluginEditor.h << 'EOF'
#pragma once
#include "PluginProcessor.h"

class SnobbyEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit SnobbyEditor (SnobbyAudioProcessor& p);
};
EOF

cat > Source/PluginEditor.cpp << 'EOF'
#include "PluginEditor.h"

SnobbyEditor::SnobbyEditor (SnobbyAudioProcessor& p)
    : juce::GenericAudioProcessorEditor (p) {}
EOF

cat > .github/workflows/build.yml << 'EOF'
name: Build Snobby DeNoise

on:
  push:
  workflow_dispatch:

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - name: Checkout (with JUCE)
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build --config Release --parallel

      - name: Upload VST3
        uses: actions/upload-artifact@v4
        with:
          name: SnobbyDeNoise-Windows-VST3
          path: build/SnobbyDeNoise_artefacts/Release/VST3
EOF

printf 'build/\n' > .gitignore

echo "Files created:"; ls CMakeLists.txt Source .github/workflows JUCE/CMakeLists.txt
git add -A
git commit -m "Snobby DeNoise: plugin source and Windows build" || echo "Nothing new to commit"
git push
echo "DONE. Open your repo on github.com, click Actions, and wait for the green tick."
