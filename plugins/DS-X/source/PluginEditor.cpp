#include "PluginEditor.h"

namespace openx::ds {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(visualizer);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(freqSlider,       freqLabel,       "Frequency");
    setupSlider(threshSlider,     threshLabel,     "Threshold");
    setupSlider(reductionSlider,  reductionLabel,  "Range");
    setupSlider(qSlider,          qLabel,          "Bandwidth Q");
    setupSlider(lookaheadSlider,  lookaheadLabel,  "Lookahead");
    setupSlider(stereoLinkSlider, stereoLinkLabel, "Stereo Link");

    auto setupBox = [this](juce::ComboBox& box, juce::Label& label, const juce::String& text) {
        addAndMakeVisible(box);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    // Mode dropdown
    modeBox.addItem("Single Vocal", 1);
    modeBox.addItem("Allround", 2);
    setupBox(modeBox, modeLabel, "Mode:");

    // Band Mode dropdown
    bandModeBox.addItem("Wide Band", 1);
    bandModeBox.addItem("Split Band", 2);
    setupBox(bandModeBox, bandModeLabel, "Band:");

    // Filter Type dropdown
    filterTypeBox.addItem("Bandpass", 1);
    filterTypeBox.addItem("Highpass", 2);
    setupBox(filterTypeBox, filterTypeLabel, "Filter:");

    // Audition dropdown
    auditionBox.addItem("Audition: Off", 1);
    auditionBox.addItem("Audition: Sidechain", 2);
    auditionBox.addItem("Audition: Delta", 3);
    addAndMakeVisible(auditionBox);

    // Stereo Mode dropdown
    stereoModeBox.addItem("Stereo", 1);
    stereoModeBox.addItem("Mid-Only", 2);
    stereoModeBox.addItem("Side-Only", 3);
    setupBox(stereoModeBox, stereoModeLabel, "M/S:");

    lpcButton.setButtonText("VT-LPSE Residual");
    addAndMakeVisible(lpcButton);

    auto& apvts = processor.getApvts();
    freqAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "freq", freqSlider);
    threshAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    reductionAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "reduction", reductionSlider);
    qAttach           = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "q", qSlider);
    lookaheadAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookahead", lookaheadSlider);
    stereoLinkAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "stereo_link", stereoLinkSlider);

    modeAttach        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);
    bandModeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "band_mode", bandModeBox);
    filterTypeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "filter_type", filterTypeBox);
    auditionAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "audition", auditionBox);
    stereoModeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "stereo_mode", stereoModeBox);
    lpcAttach         = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "use_lpc", lpcButton);

    // Bidirectional interactive visualizer binding
    visualizer.onBandChanged = [&apvts](float freq, float thresh, float q) {
        if (auto* pF = apvts.getParameter("freq"))
            pF->setValueNotifyingHost(pF->getNormalisableRange().convertTo0to1(freq));
        if (auto* pT = apvts.getParameter("threshold"))
            pT->setValueNotifyingHost(pT->getNormalisableRange().convertTo0to1(thresh));
        if (auto* pQ = apvts.getParameter("q"))
            pQ->setValueNotifyingHost(pQ->getNormalisableRange().convertTo0to1(q));
    };

    visualizer.onGestureStarted = [&apvts]() {
        if (auto* pF = apvts.getParameter("freq")) pF->beginChangeGesture();
        if (auto* pT = apvts.getParameter("threshold")) pT->beginChangeGesture();
        if (auto* pQ = apvts.getParameter("q")) pQ->beginChangeGesture();
    };

    visualizer.onGestureEnded = [&apvts]() {
        if (auto* pF = apvts.getParameter("freq")) pF->endChangeGesture();
        if (auto* pT = apvts.getParameter("threshold")) pT->endChangeGesture();
        if (auto* pQ = apvts.getParameter("q")) pQ->endChangeGesture();
    };

    setSize(980, 590);
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
    visualizer.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    const float grDb = processor.getGainReductionDb();
    const float sib = processor.getSibilanceActivity();
    const float scDb = processor.getSidechainLevelDb();
    visualizer.updateDynamics(grDb, sib, scDb);

    if (!visualizer.isDragging()) {
        const float freq = static_cast<float>(freqSlider.getValue());
        const float thresh = static_cast<float>(threshSlider.getValue());
        const float q = static_cast<float>(qSlider.getValue());
        const bool isSplitBand = (bandModeBox.getSelectedId() == 2);
        const bool isHighpass = (filterTypeBox.getSelectedId() == 2);
        visualizer.setFilterBand(freq, thresh, q, isSplitBand, isHighpass);
    }
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header branding
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  DS-X", 18, 10, 160, 24, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Intelligent De-Esser", 178, 12, 140, 24, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(14);

    // Header Top Bar
    auto headerArea = area.removeFromTop(36);
    auto controlsHeader = headerArea.withTrimmedLeft(330);

    const int comboW = 96;
    const int gap = 8;

    auditionBox.setBounds(controlsHeader.removeFromRight(135).reduced(0, 4));
    controlsHeader.removeFromRight(gap);

    auto placeHeaderCombo = [&](juce::ComboBox& box, juce::Label& lbl, int width) {
        auto r = controlsHeader.removeFromRight(width);
        lbl.setBounds(r.removeFromLeft(38));
        box.setBounds(r.reduced(0, 4));
        controlsHeader.removeFromRight(gap);
    };

    placeHeaderCombo(filterTypeBox, filterTypeLabel, comboW + 38);
    placeHeaderCombo(bandModeBox,   bandModeLabel,   comboW + 38);
    placeHeaderCombo(modeBox,       modeLabel,       comboW + 38);

    area.removeFromTop(6);

    // Bottom Controls Deck
    auto bottomArea = area.removeFromBottom(128);
    visualizer.setBounds(area);

    // Bottom panel layout: 6 knobs + right control block (M/S + LPC)
    const int rightPanelW = 140;
    auto rightBottom = bottomArea.removeFromRight(rightPanelW).reduced(4);

    stereoModeLabel.setBounds(rightBottom.removeFromTop(18));
    stereoModeBox.setBounds(rightBottom.removeFromTop(24));
    rightBottom.removeFromTop(10);
    lpcButton.setBounds(rightBottom.removeFromTop(24));

    bottomArea.removeFromRight(10);

    const int knobWidth = bottomArea.getWidth() / 6;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(18));
        s.setBounds(r);
    };

    placeKnob(freqSlider,       freqLabel,       bottomArea.removeFromLeft(knobWidth).reduced(3));
    placeKnob(threshSlider,     threshLabel,     bottomArea.removeFromLeft(knobWidth).reduced(3));
    placeKnob(reductionSlider,  reductionLabel,  bottomArea.removeFromLeft(knobWidth).reduced(3));
    placeKnob(qSlider,          qLabel,          bottomArea.removeFromLeft(knobWidth).reduced(3));
    placeKnob(lookaheadSlider,  lookaheadLabel,  bottomArea.removeFromLeft(knobWidth).reduced(3));
    placeKnob(stereoLinkSlider, stereoLinkLabel, bottomArea.removeFromLeft(knobWidth).reduced(3));
}

} // namespace openx::ds
