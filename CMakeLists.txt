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
