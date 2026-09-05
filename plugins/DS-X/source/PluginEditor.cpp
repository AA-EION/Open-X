#include "PluginEditor.h"

namespace openx::ds {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(deessCurve);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(freqSlider,      freqLabel,      "Center Freq");
    setupSlider(threshSlider,    threshLabel,    "Threshold");
    setupSlider(reductionSlider, reductionLabel, "Max Reduct");
    setupSlider(qSlider,         qLabel,         "Bandwidth Q");

    lpcButton.setButtonText("VT-LPSE Residual");
    addAndMakeVisible(lpcButton);

    auto& apvts = processor.getApvts();
    freqAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "freq", freqSlider);
    threshAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    reductionAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "reduction", reductionSlider);
    qAttach         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "q", qSlider);
    lpcAttach       = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "use_lpc", lpcButton);

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
    deessCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    openx::ui::InteractiveEqCurve::FilterBandState state;
    state.frequency = static_cast<float>(freqSlider.getValue());
    state.gainDb = static_cast<float>(reductionSlider.getValue());
    state.q = static_cast<float>(qSlider.getValue());
    state.isDynamic = true;
    deessCurve.setBandState(state);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  DS-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("Spectral Vocal De-Esser & LPC Formant Isolator", 220, 12, 300, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(110);
    deessCurve.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 5;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(freqSlider,      freqLabel,      bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(threshSlider,    threshLabel,    bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(reductionSlider, reductionLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(qSlider,         qLabel,         bottomPanel.removeFromLeft(knobWidth).reduced(4));

    lpcButton.setBounds(bottomPanel.reduced(10, 20));
}

} // namespace openx::ds
