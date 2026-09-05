#include "PluginEditor.h"

namespace openx::limit {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(scope);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(ceilingSlider,   ceilingLabel,   "Ceiling");
    setupSlider(threshSlider,    threshLabel,    "Gain / Thresh");
    setupSlider(releaseSlider,   releaseLabel,   "Release");
    setupSlider(dispFreqSlider,  dispFreqLabel,  "CO-PDN Freq");

    dispEnableButton.setButtonText("CO-PDN Phase Center");
    addAndMakeVisible(dispEnableButton);

    auto& apvts = processor.getApvts();
    ceilingAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ceiling", ceilingSlider);
    threshAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    releaseAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);
    dispFreqAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "disp_freq", dispFreqSlider);
    dispEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "disp_enable", dispEnableButton);

    setSize(860, 520);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);
    scope.updateHistory(frames, static_cast<float>(ceilingSlider.getValue()));
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  LIMIT-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("True Peak Brickwall Limiter & CO-PDN", 220, 12, 280, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(110);
    scope.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 5;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(ceilingSlider,  ceilingLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(threshSlider,   threshLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(releaseSlider,  releaseLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(dispFreqSlider, dispFreqLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));

    dispEnableButton.setBounds(bottomPanel.reduced(10, 20));
}

} // namespace openx::limit
