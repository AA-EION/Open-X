#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/deesser_visualizer.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::ds {

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& p);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;
    openx::ui::DeEsserVisualizer visualizer;

    juce::Slider freqSlider, threshSlider, reductionSlider, qSlider, lookaheadSlider, stereoLinkSlider;
    juce::Label freqLabel, threshLabel, reductionLabel, qLabel, lookaheadLabel, stereoLinkLabel;

    juce::ComboBox modeBox;
    juce::ComboBox bandModeBox;
    juce::ComboBox filterTypeBox;
    juce::ComboBox auditionBox;
    juce::ComboBox stereoModeBox;
    juce::ToggleButton lpcButton;

    juce::Label modeLabel, bandModeLabel, filterTypeLabel, auditionLabel, stereoModeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reductionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoLinkAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> auditionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lpcAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::ds
