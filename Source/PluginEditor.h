#pragma once
#include "PluginProcessor.h"

class SnobbyEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit SnobbyEditor (SnobbyAudioProcessor& p);
};
