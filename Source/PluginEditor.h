#pragma once
#include "PluginProcessor.h"

class SnobbyLnF : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 8; }
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
};

// The whole faceplate, drawn at a fixed design size (720 x 760).
// The editor scales it, so the plugin can be resized.
class SnobbyFace : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseW = 720, baseH = 760;

    explicit SnobbyFace (SnobbyAudioProcessor&);
    ~SnobbyFace() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Meter
    {
        float pk = -90.f, hold = -90.f, avg = -90.f, hv = 0.f;
        void update (float lin, float dt);
    };

    void timerCallback() override;
    void drawLevels (juce::Graphics&);
    void drawScope (juce::Graphics&);
    void drawModules (juce::Graphics&);
    void drawBottom (juce::Graphics&);
    void drawVU (juce::Graphics&, juce::Rectangle<float>);
    void startLearn();
    void finishLearn();
    void setParam (const char* id, float value);
    static juce::Rectangle<int> clipRect() { return { 560, 120, 106, 18 }; }

    SnobbyAudioProcessor& proc;
    SnobbyLnF lnf;
    juce::Slider faders[6], knobs[3];
    juce::TextButton bypass, learn;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> fa[6], ka[3];
    std::unique_ptr<BA> ba;

    Meter inM, outM;
    float voiceOut = -18.f, gr = 0.f;
    bool seen = false, clip = false;

    float sIn[24], sOut[24];
    int learnTicks = 0;
    float learnSum[6] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnobbyFace)
};

class SnobbyEditor : public juce::AudioProcessorEditor
{
public:
    explicit SnobbyEditor (SnobbyAudioProcessor&);
    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    SnobbyFace face;
    juce::ComponentBoundsConstrainer constrainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnobbyEditor)
};
