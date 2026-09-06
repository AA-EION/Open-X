#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace openx::mb {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(multibandCurve);

    auto& apvts = processor.getApvts();

    // Setup helper for sliders
    // Helper for header gain LinearBar sliders
    auto setupGainBar = [this](juce::Slider& slider, juce::Label& label, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff212836));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff).withAlpha(0.25f));
        slider.textFromValueFunction = [](double v) {
            return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
        };
        addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff828997));
        addAndMakeVisible(label);
    };

    // Helper for crossover LinearBar sliders
    auto setupXoBar = [this](juce::Slider& slider, juce::Label& label, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff212836));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff).withAlpha(0.25f));
        slider.textFromValueFunction = [](double val) {
            if (val >= 1000.0) return juce::String(val / 1000.0, 1) + " kHz";
            return juce::String(static_cast<int>(std::round(val))) + " Hz";
        };
        slider.valueFromTextFunction = [](const juce::String& str) {
            juce::String t = str.trim().toLowerCase();
            if (t.endsWith("khz")) return t.dropLastCharacters(3).getDoubleValue() * 1000.0;
            if (t.endsWith("k")) return t.dropLastCharacters(1).getDoubleValue() * 1000.0;
            if (t.endsWith("hz")) return t.dropLastCharacters(2).getDoubleValue();
            return t.getDoubleValue();
        };
        addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff828997));
        addAndMakeVisible(label);
    };

    // Header Global Knobs
    setupGainBar(inGainSlider, inGainLabel, "In Gain");
    setupGainBar(outGainSlider, outGainLabel, "Out Gain");
    inGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "input_gain", inGainSlider);
    outGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "output_gain", outGainSlider);

    // Crossover Quick Knobs
    setupXoBar(xo1Slider, xo1Label, "XO Low-Mid");
    setupXoBar(xo2Slider, xo2Label, "XO Mid-High");
    setupXoBar(xo3Slider, xo3Label, "XO High-Air");
    xo1Attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "xo1", xo1Slider);
    xo2Attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "xo2", xo2Slider);
    xo3Attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "xo3", xo3Slider);

    // Band Tab Buttons
    static constexpr const char* TabNames[4] = { "1: LOW", "2: LOW-MID", "3: HIGH-MID", "4: HIGH" };
    static constexpr uint32_t TabColours[4] = { 0xffff5252, 0xffffb74d, 0xff00e5ff, 0xffb388ff };

    for (size_t b = 0; b < 4; ++b) {
        auto& btn = bandTabButtons[b];
        btn.setButtonText(TabNames[b]);
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x1a212a));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(TabColours[b]));
        btn.onClick = [this, b]() {
            selectBand(b);
        };
        addAndMakeVisible(btn);
    }

    // Per-Band Inspector Panels & Attachments
    for (size_t b = 0; b < 4; ++b) {
        auto& bc = bandControls[b];
        addAndMakeVisible(bc.panel);

        const juce::String prefix = "b" + juce::String(static_cast<int>(b)) + "_";

        // Mode Button: Compress / Expand
        bc.modeButton.setClickingTogglesState(true);
        bc.modeButton.setButtonText("COMPRESS");
        bc.modeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff212836));
        bc.modeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe5c07b));
        bc.modeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        bc.modeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        bc.panel.addAndMakeVisible(bc.modeButton);
        bc.modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "mode", bc.modeButton);

        // Solo Button
        bc.soloButton.setClickingTogglesState(true);
        bc.soloButton.setButtonText("S");
        bc.soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff212836));
        bc.soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffb300));
        bc.soloButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffabb2bf));
        bc.soloButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        bc.panel.addAndMakeVisible(bc.soloButton);
        bc.soloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "solo", bc.soloButton);

        // Mute Button
        bc.muteButton.setClickingTogglesState(true);
        bc.muteButton.setButtonText("M");
        bc.muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff212836));
        bc.muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff5252));
        bc.muteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffabb2bf));
        bc.muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        bc.panel.addAndMakeVisible(bc.muteButton);
        bc.muteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "mute", bc.muteButton);

        // Bypass Button
        bc.bypassButton.setClickingTogglesState(true);
        bc.bypassButton.setButtonText("BYP");
        bc.bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff212836));
        bc.bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5c6370));
        bc.bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffabb2bf));
        bc.bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        bc.panel.addAndMakeVisible(bc.bypassButton);
        bc.bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "bypass", bc.bypassButton);

        // Knobs setup within panel
        auto setupPanelKnob = [&bc](juce::Slider& s, juce::Label& l, const juce::String& name) {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 15);
            s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffabb2bf));
            bc.panel.addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            l.setColour(juce::Label::textColourId, juce::Colour(0xff828997));
            bc.panel.addAndMakeVisible(l);
        };

        setupPanelKnob(bc.threshSlider,  bc.threshLabel,  "Threshold");
        setupPanelKnob(bc.rangeSlider,   bc.rangeLabel,   "Range");
        setupPanelKnob(bc.ratioSlider,   bc.ratioLabel,   "Ratio");
        setupPanelKnob(bc.attackSlider,  bc.attackLabel,  "Attack");
        setupPanelKnob(bc.releaseSlider, bc.releaseLabel, "Release");
        setupPanelKnob(bc.kneeSlider,    bc.kneeLabel,    "Knee");
        setupPanelKnob(bc.gainSlider,    bc.gainLabel,    "Makeup");

        bc.threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "thresh",  bc.threshSlider);
        bc.rangeAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "range",   bc.rangeSlider);
        bc.ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "ratio",   bc.ratioSlider);
        bc.attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "attack",  bc.attackSlider);
        bc.releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "release", bc.releaseSlider);
        bc.kneeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "knee",    bc.kneeSlider);
        bc.gainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "gain",    bc.gainSlider);
    }

    // Connect MultibandCurve Callbacks
    multibandCurve.onBandSelected = [this](size_t b) {
        selectBand(b);
    };

    multibandCurve.onCrossoverChanged = [this](size_t idx, float newFreq) {
        if (idx == 0) xo1Slider.setValue(newFreq, juce::sendNotificationSync);
        else if (idx == 1) xo2Slider.setValue(newFreq, juce::sendNotificationSync);
        else if (idx == 2) xo3Slider.setValue(newFreq, juce::sendNotificationSync);
    };

    multibandCurve.onBandGainChanged = [this](size_t b, float newGainDb) {
        if (b < 4) {
            bandControls[b].gainSlider.setValue(newGainDb, juce::sendNotificationSync);
        }
    };

    selectBand(0);

    setSize(1000, 680);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditor::selectBand(size_t index) {
    if (index >= 4) return;
    activeBandIndex = index;
    multibandCurve.setSelectedBand(index);

    static constexpr uint32_t TabColours[4] = { 0xffff5252, 0xffffb74d, 0xff00e5ff, 0xffb388ff };

    for (size_t b = 0; b < 4; ++b) {
        const bool isSelected = (b == index);
        bandControls[b].panel.setVisible(isSelected);

        auto& tabBtn = bandTabButtons[b];
        if (isSelected) {
            tabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(TabColours[b]).withAlpha(0.25f));
            tabBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        } else {
            tabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x1a212a));
            tabBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(TabColours[b]).withAlpha(0.7f));
        }
    }
    repaint();
}

void PluginEditor::timerCallback() {
    // 1. Update Spectrum Analyzer
    auto& analyzer = processor.getSpectrumAnalyzer();
    const float sr = static_cast<float>(processor.getSampleRate());
    analyzer.update(sr > 0 ? sr : 48000.0f);

    auto scopeData = analyzer.getScopeData();
    multibandCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    // 2. Sync Crossover Frequencies to Curve
    multibandCurve.setCrossoverFrequency(0, static_cast<float>(xo1Slider.getValue()));
    multibandCurve.setCrossoverFrequency(1, static_cast<float>(xo2Slider.getValue()));
    multibandCurve.setCrossoverFrequency(2, static_cast<float>(xo3Slider.getValue()));

    // 3. Sync Band Dynamic States and Real-Time Gain Reduction
    for (size_t b = 0; b < 4; ++b) {
        auto& bc = bandControls[b];
        const float grDb = processor.getBandGainReductionDb(b);
        multibandCurve.setBandGainChange(b, grDb);

        openx::ui::MultibandCurve::BandState state;
        state.makeupGainDb = static_cast<float>(bc.gainSlider.getValue());
        state.thresholdDb  = static_cast<float>(bc.threshSlider.getValue());
        state.rangeDb      = static_cast<float>(bc.rangeSlider.getValue());
        state.currentGainChangeDb = grDb;
        state.mode   = bc.modeButton.getToggleState() ? 1 : 0;
        state.solo   = bc.soloButton.getToggleState();
        state.mute   = bc.muteButton.getToggleState();
        state.bypass = bc.bypassButton.getToggleState();

        static constexpr uint32_t BandColours[4] = { 0xffff5252, 0xffffb74d, 0xff00e5ff, 0xffb388ff };
        static constexpr const char* BandNames[4] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };
        state.colour = juce::Colour(BandColours[b]);
        state.name = BandNames[b];

        multibandCurve.setBandState(b, state);

        // Update mode button text
        const juce::String expectedModeText = bc.modeButton.getToggleState() ? "EXPAND" : "COMPRESS";
        if (bc.modeButton.getButtonText() != expectedModeText) {
            bc.modeButton.setButtonText(expectedModeText);
        }
    }
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header bar background
    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), 46);
    g.setColour(juce::Colour(0xff161a22));
    g.fillRect(headerRect);
    g.setColour(juce::Colour(0xff282c34));
    g.drawHorizontalLine(46, 0.0f, static_cast<float>(getWidth()));

    // Title & branding
    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  MB-X", 20, 8, 180, 30, juce::Justification::left);

    g.setColour(juce::Colour(0xff828997));
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Professional Multiband Dynamics Processor", 195, 10, 320, 30, juce::Justification::left);

    // Sub-header band selector bar background
    auto subHeaderRect = juce::Rectangle<int>(0, 46, getWidth(), 40);
    g.setColour(juce::Colour(0xff181c24));
    g.fillRect(subHeaderRect);
    g.setColour(juce::Colour(0xff212630));
    g.drawHorizontalLine(86, 0.0f, static_cast<float>(getWidth()));

    // Inspector panel background
    const int inspectorY = getHeight() - 170;
    auto inspectorRect = juce::Rectangle<int>(0, inspectorY, getWidth(), 170);
    g.setColour(juce::Colour(0xff161920));
    g.fillRect(inspectorRect);
    g.setColour(juce::Colour(0xff282c34));
    g.drawHorizontalLine(inspectorY, 0.0f, static_cast<float>(getWidth()));
}

void PluginEditor::resized() {
    const int w = getWidth();
    const int h = getHeight();

    // 1. Top Header: Global Gain LinearBar Sliders
    const int gainKnobW = 75;
    inGainLabel.setBounds(w - 180, 4, gainKnobW, 14);
    inGainSlider.setBounds(w - 180, 18, gainKnobW, 24);
    outGainLabel.setBounds(w - 90, 4, gainKnobW, 14);
    outGainSlider.setBounds(w - 90, 18, gainKnobW, 24);

    // 2. Sub-Header: Band Tabs (Left) & Crossover Knobs (Right)
    const int tabY = 50;
    const int tabH = 32;
    const int tabW = 105;
    for (size_t b = 0; b < 4; ++b) {
        bandTabButtons[b].setBounds(16 + static_cast<int>(b) * (tabW + 6), tabY, tabW, tabH);
    }

    // Crossover Quick Knobs on the right side of sub-header
    const int xoKnobW = 86;
    const int xoStartX = w - (3 * xoKnobW + 20);
    auto placeXo = [&](juce::Slider& s, juce::Label& l, int x) {
        l.setBounds(x, 48, xoKnobW, 13);
        s.setBounds(x, 61, xoKnobW, 22);
    };
    placeXo(xo1Slider, xo1Label, xoStartX);
    placeXo(xo2Slider, xo2Label, xoStartX + xoKnobW + 4);
    placeXo(xo3Slider, xo3Label, xoStartX + (xoKnobW + 4) * 2);

    // 3. Main Multiband Curve Display
    const int curveY = 88;
    const int inspectorH = 170;
    const int curveH = h - curveY - inspectorH - 6;
    multibandCurve.setBounds(12, curveY, w - 24, curveH);

    // 4. Inspector Panels
    const int inspectorY = h - inspectorH;
    const auto inspectorArea = juce::Rectangle<int>(12, inspectorY + 6, w - 24, inspectorH - 12);

    for (size_t b = 0; b < 4; ++b) {
        auto& bc = bandControls[b];
        bc.panel.setBounds(inspectorArea);

        auto panelBounds = bc.panel.getLocalBounds();

        // Left Action Strip: Mode, Solo, Mute, Bypass
        auto leftBox = panelBounds.removeFromLeft(180).reduced(6);

        bc.modeButton.setBounds(leftBox.removeFromTop(36));
        leftBox.removeFromTop(10);

        auto btnRow = leftBox.removeFromTop(32);
        const int btnW = (btnRow.getWidth() - 10) / 3;
        bc.soloButton.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(5);
        bc.muteButton.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(5);
        bc.bypassButton.setBounds(btnRow);

        panelBounds.removeFromLeft(16); // Gap

        // Right Strip: 7 Rotary Knobs
        const int knobCount = 7;
        const int knobWidth = panelBounds.getWidth() / knobCount;

        auto placeInspectorKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
            l.setBounds(r.removeFromTop(20));
            s.setBounds(r);
        };

        placeInspectorKnob(bc.threshSlider,  bc.threshLabel,  panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.rangeSlider,   bc.rangeLabel,   panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.ratioSlider,   bc.ratioLabel,   panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.attackSlider,  bc.attackLabel,  panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.releaseSlider, bc.releaseLabel, panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.kneeSlider,    bc.kneeLabel,    panelBounds.removeFromLeft(knobWidth).reduced(4));
        placeInspectorKnob(bc.gainSlider,    bc.gainLabel,    panelBounds.removeFromLeft(knobWidth).reduced(4));
    }
}

} // namespace openx::mb
