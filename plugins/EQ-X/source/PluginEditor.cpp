#include "PluginEditor.h"

namespace openx::eq {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    // 1. Interactive EQ Curve Component
    addAndMakeVisible(eqCurve);

    eqCurve.onBandSelected = [this](size_t bandIdx) {
        selectBand(bandIdx);
    };

    eqCurve.onBandChanged = [this](size_t bandIdx, float freq, float gainDb, float q) {
        if (bandIdx == selectedBand) {
            freqSlider.setValue(freq, juce::sendNotificationSync);
            gainSlider.setValue(gainDb, juce::sendNotificationSync);
            qSlider.setValue(q, juce::sendNotificationSync);
        } else {
            auto& apvts = processor.getApvts();
            if (auto* pf = apvts.getParameter(getBandFreqIdStr(bandIdx))) {
                pf->setValueNotifyingHost(pf->convertTo0to1(freq));
            }
            if (auto* pg = apvts.getParameter(getBandGainIdStr(bandIdx))) {
                pg->setValueNotifyingHost(pg->convertTo0to1(gainDb));
            }
            if (auto* pq = apvts.getParameter(getBandQIdStr(bandIdx))) {
                pq->setValueNotifyingHost(pq->convertTo0to1(q));
            }
        }
    };

    eqCurve.onBandDynamicsChanged = [this](size_t bandIdx, float dynGainDb, float threshDb) {
        if (bandIdx == selectedBand) {
            dynGainSlider.setValue(dynGainDb, juce::sendNotificationSync);
            threshSlider.setValue(threshDb, juce::sendNotificationSync);
        } else {
            auto& apvts = processor.getApvts();
            if (auto* pd = apvts.getParameter(getBandDynGainIdStr(bandIdx))) {
                pd->setValueNotifyingHost(pd->convertTo0to1(dynGainDb));
            }
            if (auto* pt = apvts.getParameter(getBandThresholdIdStr(bandIdx))) {
                pt->setValueNotifyingHost(pt->convertTo0to1(threshDb));
            }
        }
    };

    eqCurve.onBandBypassToggled = [this](size_t bandIdx, bool bypassed) {
        if (auto* pb = processor.getApvts().getParameter(getBandBypassIdStr(bandIdx))) {
            pb->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);
        }
        if (bandIdx == selectedBand) {
            bypassButton.setToggleState(bypassed, juce::dontSendNotification);
        }
    };

    eqCurve.onBandSoloToggled = [this](size_t bandIdx, bool solo) {
        if (auto* ps = processor.getApvts().getParameter(getBandSoloIdStr(bandIdx))) {
            ps->setValueNotifyingHost(solo ? 1.0f : 0.0f);
        }
        if (bandIdx == selectedBand) {
            soloButton.setToggleState(solo, juce::dontSendNotification);
        }
    };

    eqCurve.onSingleBandChanged = [this](float freq, float gainDb, float q) {
        freqSlider.setValue(freq, juce::sendNotificationSync);
        gainSlider.setValue(gainDb, juce::sendNotificationSync);
        qSlider.setValue(q, juce::sendNotificationSync);
    };

    // 2. Rotary slider setup helper
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    // 3. Master In / Out Gain controls
    auto setupMasterSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupMasterSlider(inGainSlider, inGainLabel, "IN GAIN");
    setupMasterSlider(outGainSlider, outGainLabel, "OUT GAIN");

    inGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getApvts(), "in_gain", inGainSlider);
    outGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getApvts(), "out_gain", outGainSlider);

    // 4. Band selector toolbar (Bands 1 to 8)
    bandsBarLabel.setText("BANDS", juce::dontSendNotification);
    bandsBarLabel.setJustificationType(juce::Justification::centredLeft);
    bandsBarLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    bandsBarLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::TextMuted);
    addAndMakeVisible(bandsBarLabel);

    for (size_t i = 0; i < NumBands; ++i) {
        bandButtons[i].setButtonText(juce::String(static_cast<int>(i + 1)));
        const auto c = openx::ui::InteractiveEqCurve::getBandColour(i);
        bandButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1e24));
        bandButtons[i].setColour(juce::TextButton::buttonOnColourId, c);
        bandButtons[i].setColour(juce::TextButton::textColourOffId, c);
        bandButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xff121418));
        bandButtons[i].setClickingTogglesState(false);
        bandButtons[i].onClick = [this, i]() {
            selectBand(i);
        };
        addAndMakeVisible(bandButtons[i]);
    }

    // 5. Selected Band Inspector Panel
    bandBadgeLabel.setText("BAND 1", juce::dontSendNotification);
    bandBadgeLabel.setJustificationType(juce::Justification::centredLeft);
    bandBadgeLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(bandBadgeLabel);

    typeComboBox.addItem("Bell", 1);
    typeComboBox.addItem("Low Shelf", 2);
    typeComboBox.addItem("High Shelf", 3);
    typeComboBox.addItem("Notch", 4);
    typeComboBox.addItem("Low Cut", 5);
    typeComboBox.addItem("High Cut", 6);
    typeComboBox.onChange = [this]() {
        const int id = typeComboBox.getSelectedId();
        if (id >= 1 && id <= 6 && !isUpdatingFromTimer) {
            const float val = static_cast<float>(id - 1);
            if (auto* param = processor.getApvts().getParameter(getBandTypeIdStr(selectedBand))) {
                param->setValueNotifyingHost(param->convertTo0to1(val));
            }
            const bool usesGain = openx::ui::InteractiveEqCurve::filterTypeUsesGain(
                static_cast<openx::dsp::DynamicBiquadEngine<float>::FilterType>(id - 1));
            gainSlider.setEnabled(usesGain);
            gainLabel.setEnabled(usesGain);
        }
    };
    addAndMakeVisible(typeComboBox);

    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(juce::ToggleButton::textColourId, openx::ui::OpenXLookAndFeel::TextPrimary);
    bypassButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff1744));
    addAndMakeVisible(bypassButton);

    soloButton.setButtonText("Solo");
    soloButton.setColour(juce::ToggleButton::textColourId, openx::ui::OpenXLookAndFeel::TextPrimary);
    soloButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffd600));
    addAndMakeVisible(soloButton);

    setupSlider(freqSlider, freqLabel, "Freq");
    setupSlider(gainSlider, gainLabel, "Gain");
    setupSlider(qSlider, qLabel, "Q");
    setupSlider(dynGainSlider, dynGainLabel, "Dyn Gain");
    setupSlider(threshSlider, threshLabel, "Threshold");

    // 6. Select initial band & start timer
    selectBand(0);

    setSize(980, 640);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::selectBand(size_t bandIdx) {
    if (bandIdx >= NumBands) return;
    selectedBand = bandIdx;

    // 1. Release existing parameter attachments
    freqAttach.reset();
    gainAttach.reset();
    qAttach.reset();
    dynGainAttach.reset();
    threshAttach.reset();
    bypassAttach.reset();
    soloAttach.reset();

    auto& apvts = processor.getApvts();
    const auto& params = processor.getParamManager();

    // 2. Re-bind attachments for the selected band
    freqAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, getBandFreqIdStr(selectedBand), freqSlider);
    gainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, getBandGainIdStr(selectedBand), gainSlider);
    qAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, getBandQIdStr(selectedBand), qSlider);
    dynGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, getBandDynGainIdStr(selectedBand), dynGainSlider);
    threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, getBandThresholdIdStr(selectedBand), threshSlider);
    bypassAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, getBandBypassIdStr(selectedBand), bypassButton);
    soloAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, getBandSoloIdStr(selectedBand), soloButton);

    // 3. Update Type combo box selection
    const int typeVal = std::clamp(static_cast<int>(std::round(params.get(getBandTypeId(selectedBand)))), 0, 5);
    isUpdatingFromTimer = true;
    typeComboBox.setSelectedId(typeVal + 1, juce::dontSendNotification);
    const bool usesGain = openx::ui::InteractiveEqCurve::filterTypeUsesGain(
        static_cast<openx::dsp::DynamicBiquadEngine<float>::FilterType>(typeVal));
    gainSlider.setEnabled(usesGain);
    gainLabel.setEnabled(usesGain);
    isUpdatingFromTimer = false;

    // 4. Update Band Badge Label
    const auto bandColor = openx::ui::InteractiveEqCurve::getBandColour(selectedBand);
    bandBadgeLabel.setText("BAND " + juce::String(static_cast<int>(selectedBand + 1)), juce::dontSendNotification);
    bandBadgeLabel.setColour(juce::Label::textColourId, bandColor);

    // 5. Update Band Buttons visual toggle state
    for (size_t i = 0; i < NumBands; ++i) {
        bandButtons[i].setToggleState(i == selectedBand, juce::dontSendNotification);
    }

    // 6. Notify EQ curve
    eqCurve.setSelectedBand(selectedBand);
    repaint();
}

void PluginEditor::timerCallback() {
    auto& analyzer = processor.getSpectrumAnalyzer();
    const float sr = static_cast<float>(processor.getSampleRate());
    analyzer.update(sr > 0 ? sr : 48000.0f);

    auto scopeData = analyzer.getScopeData();
    eqCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    const auto& params = processor.getParamManager();

    // Push all 8 band states to the interactive curve
    for (size_t b = 0; b < NumBands; ++b) {
        openx::ui::InteractiveEqCurve::FilterBandState state;
        state.frequency          = params.get(getBandFreqId(b));
        state.gainDb             = params.get(getBandGainId(b));
        state.q                  = params.get(getBandQId(b));
        state.dynamicGainDb      = params.get(getBandDynGainId(b));
        state.dynamicThresholdDb = params.get(getBandThresholdId(b));

        const int tInt           = std::clamp(static_cast<int>(std::round(params.get(getBandTypeId(b)))), 0, 5);
        state.type               = static_cast<openx::dsp::DynamicBiquadEngine<float>::FilterType>(tInt);
        state.bypassed           = (params.get(getBandBypassId(b)) > 0.5f);
        state.solo               = (params.get(getBandSoloId(b)) > 0.5f);
        state.isDynamic          = (std::abs(state.dynamicGainDb) > 0.1f);
        state.dynamicOffsetDb    = processor.getDynamicGainOffset(b);

        eqCurve.setBandState(b, state);
    }

    // Synchronize type combo box if automation changed it externally
    const int curType = std::clamp(static_cast<int>(std::round(params.get(getBandTypeId(selectedBand)))), 0, 5);
    if (typeComboBox.getSelectedId() != curType + 1) {
        isUpdatingFromTimer = true;
        typeComboBox.setSelectedId(curType + 1, juce::dontSendNotification);
        const bool usesGain = openx::ui::InteractiveEqCurve::filterTypeUsesGain(
            static_cast<openx::dsp::DynamicBiquadEngine<float>::FilterType>(curType));
        gainSlider.setEnabled(usesGain);
        gainLabel.setEnabled(usesGain);
        isUpdatingFromTimer = false;
    }
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header Background
    const auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), 46);
    g.setColour(juce::Colour(0xff161a20));
    g.fillRect(headerBounds);
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawHorizontalLine(46, 0.0f, static_cast<float>(getWidth()));

    // Title & Subtitle banner
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  EQ-X", 18, 8, 180, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("8-Band Dynamic Parametric Equalizer", 195, 10, 240, 28, juce::Justification::left);

    // Bottom Inspector Panel Background
    const auto bottomBounds = juce::Rectangle<int>(0, getHeight() - 128, getWidth(), 128);
    g.setColour(juce::Colour(0xff161a20));
    g.fillRect(bottomBounds);
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawHorizontalLine(getHeight() - 128, 0.0f, static_cast<float>(getWidth()));
    g.drawVerticalLine(190, static_cast<float>(getHeight() - 128), static_cast<float>(getHeight()));
}

void PluginEditor::resized() {
    auto area = getLocalBounds();

    // 1. Top Header
    auto headerArea = area.removeFromTop(46).reduced(16, 2);
    headerArea.removeFromLeft(440); // Leave room for title & subtitle
    auto outGainArea = headerArea.removeFromRight(70);
    auto inGainArea  = headerArea.removeFromRight(70);

    auto placeMasterKnob = [](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(14));
        s.setBounds(r);
    };
    placeMasterKnob(inGainSlider, inGainLabel, inGainArea);
    placeMasterKnob(outGainSlider, outGainLabel, outGainArea);

    // 2. Band Selector Bar
    auto bandBar = area.removeFromTop(36).reduced(16, 3);
    bandsBarLabel.setBounds(bandBar.removeFromLeft(56));
    const int btnWidth = 44;
    for (size_t i = 0; i < NumBands; ++i) {
        bandButtons[i].setBounds(bandBar.removeFromLeft(btnWidth).reduced(2, 1));
    }

    // 3. Bottom Inspector Panel
    auto bottomPanel = area.removeFromBottom(128).reduced(16, 8);

    auto inspectorLeft = bottomPanel.removeFromLeft(160).reduced(2);
    bandBadgeLabel.setBounds(inspectorLeft.removeFromTop(22));
    typeComboBox.setBounds(inspectorLeft.removeFromTop(28).reduced(0, 2));

    auto buttonRow = inspectorLeft.removeFromTop(28).reduced(0, 2);
    bypassButton.setBounds(buttonRow.removeFromLeft(76));
    soloButton.setBounds(buttonRow);

    bottomPanel.removeFromLeft(20); // Spacer

    const int knobWidth = bottomPanel.getWidth() / 5;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(18));
        s.setBounds(r);
    };

    placeKnob(freqSlider, freqLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(gainSlider, gainLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(qSlider, qLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(dynGainSlider, dynGainLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(threshSlider, threshLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));

    // 4. Center EQ Curve area
    eqCurve.setBounds(area.reduced(16, 4));
}

} // namespace openx::eq
