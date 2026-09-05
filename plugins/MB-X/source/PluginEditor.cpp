#include "PluginEditor.h"

namespace openx::mb {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(multibandCurve);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 16);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(xo1Slider, xo1Label, "Low-Mid XO");
    setupSlider(xo2Slider, xo2Label, "Mid-High XO");
    setupSlider(lowThreshSlider, lowThreshLabel, "Low Thresh");
    setupSlider(midThreshSlider, midThreshLabel, "Mid Thresh");
    setupSlider(hiThreshSlider, hiThreshLabel, "High Thresh");
    setupSlider(lowGainSlider, lowGainLabel, "Low Gain");
    setupSlider(midGainSlider, midGainLabel, "Mid Gain");
    setupSlider(hiGainSlider, hiGainLabel, "High Gain");

    auto& apvts = processor.getApvts();
    xo1Attach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "xo1", xo1Slider);
    xo2Attach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "xo2", xo2Slider);
    lowThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "low_thresh", lowThreshSlider);
    midThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid_thresh", midThreshSlider);
    hiThreshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hi_thresh", hiThreshSlider);
    lowGainAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "low_gain", lowGainSlider);
    midGainAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid_gain", midGainSlider);
    hiGainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hi_gain", hiGainSlider);

    setSize(920, 540);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    auto& analyzer = processor.getSpectrumAnalyzer();
    const float sr = static_cast<float>(processor.getSampleRate());
    analyzer.update(sr > 0 ? sr : 48000.0f);

    auto scopeData = analyzer.getScopeData();
    multibandCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    openx::ui::InteractiveEqCurve::FilterBandState state;
    state.frequency = static_cast<float>(xo1Slider.getValue());
    state.gainDb = static_cast<float>(lowGainSlider.getValue());
    state.q = 0.7071f;
    state.isDynamic = true;
    multibandCurve.setBandState(state);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  MB-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Phase-Aligned Dynamic Multiband Processor", 200, 12, 300, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(120);
    multibandCurve.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 8;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(xo1Slider,       xo1Label,       bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(xo2Slider,       xo2Label,       bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(lowThreshSlider, lowThreshLabel, bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(midThreshSlider, midThreshLabel, bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(hiThreshSlider,  hiThreshLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(lowGainSlider,   lowGainLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(midGainSlider,   midGainLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(3));
    placeKnob(hiGainSlider,    hiGainLabel,    bottomPanel.removeFromLeft(knobWidth).reduced(3));
}

} // namespace openx::mb
