#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/multiband_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"
#include <array>
#include <memory>

namespace openx::mb {

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& p);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectBand(size_t index);

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;

    // Main Interactive Display
    openx::ui::MultibandCurve multibandCurve;

    // Header Global Controls
    juce::Slider inGainSlider, outGainSlider;
    juce::Label inGainLabel, outGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach;

    // Crossover Quick Knobs
    juce::Slider xo1Slider, xo2Slider, xo3Slider;
    juce::Label xo1Label, xo2Label, xo3Label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xo1Attach, xo2Attach, xo3Attach;

    // Band Selector Tab Buttons
    std::array<juce::TextButton, 4> bandTabButtons;
    size_t activeBandIndex{0};

    // Per-Band Inspector Controls
    struct BandControls {
        juce::Component panel;
        juce::TextButton modeButton;
        juce::TextButton soloButton;
        juce::TextButton muteButton;
        juce::TextButton bypassButton;

        juce::Slider threshSlider;
        juce::Slider rangeSlider;
        juce::Slider ratioSlider;
        juce::Slider attackSlider;
        juce::Slider releaseSlider;
        juce::Slider kneeSlider;
        juce::Slider gainSlider;

        juce::Label threshLabel;
        juce::Label rangeLabel;
        juce::Label ratioLabel;
        juce::Label attackLabel;
        juce::Label releaseLabel;
        juce::Label kneeLabel;
        juce::Label gainLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;
    };

    std::array<BandControls, 4> bandControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::mb
