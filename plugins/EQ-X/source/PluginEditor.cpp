#include "PluginEditor.h"

namespace openx::eq {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(eqCurve);
    eqCurve.onBandChanged = [this](float freq, float gainDb, float q) {
        freqSlider.setValue(freq, juce::sendNotificationSync);
        gainSlider.setValue(gainDb, juce::sendNotificationSync);
        qSlider.setValue(q, juce::sendNotificationSync);
    };

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(freqSlider, freqLabel, "Freq");
    setupSlider(gainSlider, gainLabel, "Gain");
    setupSlider(qSlider, qLabel, "Q");
    setupSlider(dynGainSlider, dynGainLabel, "Dyn Gain");
    setupSlider(threshSlider, threshLabel, "Threshold");

    auto& apvts = processor.getApvts();
    freqAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "b1_freq", freqSlider);
    gainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "b1_gain", gainSlider);
    qAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "b1_q", qSlider);
    dynGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "b1_dyngain", dynGainSlider);
    threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "b1_thresh", threshSlider);

    setSize(860, 520);
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
    eqCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    openx::ui::InteractiveEqCurve::FilterBandState state;
    state.frequency = static_cast<float>(freqSlider.getValue());
    state.gainDb = static_cast<float>(gainSlider.getValue());
    state.q = static_cast<float>(qSlider.getValue());
    state.dynamicGainDb = static_cast<float>(dynGainSlider.getValue());
    state.isDynamic = (std::abs(state.dynamicGainDb) > 0.1f);
    eqCurve.setBandState(state);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Title banner
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  EQ-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Dynamic Spectral Equalizer", 200, 12, 200, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32); // Header

    auto bottomPanel = area.removeFromBottom(110);
    eqCurve.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 5;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(freqSlider, freqLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(gainSlider, gainLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(qSlider, qLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(dynGainSlider, dynGainLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(threshSlider, threshLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
}

} // namespace openx::eq
