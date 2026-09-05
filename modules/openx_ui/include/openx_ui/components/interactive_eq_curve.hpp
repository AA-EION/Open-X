#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <openx_dsp/eq/dynamic_biquad_engine.hpp>
#include "../dsp/spectrum_analyzer.hpp"
#include <functional>
#include <cmath>
#include <array>
#include <algorithm>

namespace openx::ui {

class InteractiveEqCurve : public juce::Component {
public:
    static constexpr size_t NumBands = 8;
    using FilterType = openx::dsp::DynamicBiquadEngine<float>::FilterType;

    struct FilterBandState {
        float frequency{1000.0f};
        float gainDb{0.0f};
        float q{0.7071f};
        float dynamicGainDb{0.0f};
        float dynamicThresholdDb{-20.0f};
        FilterType type{FilterType::Bell};
        bool isDynamic{false};
        bool bypassed{false};
        bool solo{false};
        float dynamicOffsetDb{0.0f};
    };

    std::function<void(size_t bandIdx, float freq, float gainDb, float q)> onBandChanged;
    std::function<void(size_t bandIdx)> onBandSelected;
    std::function<void(size_t bandIdx, bool bypassed)> onBandBypassToggled;
    std::function<void(size_t bandIdx, bool solo)> onBandSoloToggled;
    std::function<void(size_t bandIdx, float dynGainDb, float threshDb)> onBandDynamicsChanged;
    std::function<void(float freq, float gainDb, float q)> onSingleBandChanged;

    InteractiveEqCurve() {
        setInterceptsMouseClicks(true, false);
        initDefaultBands();
    }

    void initDefaultBands() noexcept {
        static constexpr float defaultFreqs[NumBands] = {
            30.0f, 80.0f, 200.0f, 600.0f, 1500.0f, 4000.0f, 9000.0f, 16000.0f
        };
        static constexpr FilterType defaultTypes[NumBands] = {
            FilterType::LowShelf, FilterType::Bell, FilterType::Bell, FilterType::Bell,
            FilterType::Bell, FilterType::Bell, FilterType::Bell, FilterType::HighShelf
        };

        for (size_t i = 0; i < NumBands; ++i) {
            bands[i].frequency = defaultFreqs[i];
            bands[i].gainDb = 0.0f;
            bands[i].q = 0.7071f;
            bands[i].dynamicGainDb = 0.0f;
            bands[i].dynamicThresholdDb = -20.0f;
            bands[i].type = defaultTypes[i];
            bands[i].isDynamic = false;
            bands[i].bypassed = false;
            bands[i].solo = false;
            bands[i].dynamicOffsetDb = 0.0f;
        }
    }

    void setBandState(size_t bandIdx, const FilterBandState& state) noexcept {
        if (bandIdx < NumBands) {
            // While actively dragging this band, protect its position from timer jitter
            if (draggingBand == static_cast<int>(bandIdx)) {
                bands[bandIdx].dynamicOffsetDb = state.dynamicOffsetDb;
                bands[bandIdx].isDynamic = state.isDynamic;
                bands[bandIdx].bypassed = state.bypassed;
                bands[bandIdx].solo = state.solo;
            } else {
                bands[bandIdx] = state;
            }
            repaint();
        }
    }

    void setBandState(const FilterBandState& state) noexcept {
        setBandState(selectedBand, state);
    }

    [[nodiscard]] const FilterBandState& getBandState(size_t bandIdx) const noexcept {
        jassert(bandIdx < NumBands);
        return bands[bandIdx];
    }

    void setSelectedBand(size_t bandIdx) noexcept {
        if (bandIdx < NumBands && selectedBand != bandIdx) {
            selectedBand = bandIdx;
            repaint();
        }
    }

    [[nodiscard]] size_t getSelectedBand() const noexcept {
        return selectedBand;
    }

    void updateSpectrum(const float* scopeData, size_t numBins, float sampleRate) noexcept {
        if (scopeData != nullptr && numBins > 0) {
            spectrumBins = std::min(numBins, spectrumData.size());
            std::copy_n(scopeData, spectrumBins, spectrumData.begin());
            hasSpectrum = true;
        } else {
            hasSpectrum = false;
        }
        sr = sampleRate;
        repaint();
    }

    [[nodiscard]] static bool filterTypeUsesGain(FilterType type) noexcept {
        return type == FilterType::Bell || type == FilterType::LowShelf || type == FilterType::HighShelf;
    }

    [[nodiscard]] static const char* getFilterTypeName(FilterType type) noexcept {
        switch (type) {
            case FilterType::Bell:      return "Bell";
            case FilterType::LowShelf:  return "Low Shelf";
            case FilterType::HighShelf: return "High Shelf";
            case FilterType::Notch:     return "Notch";
            case FilterType::LowCut:    return "Low Cut";
            case FilterType::HighCut:   return "High Cut";
            default:                    return "Bell";
        }
    }

    [[nodiscard]] static juce::Colour getBandColour(size_t bandIdx) noexcept {
        static constexpr uint32_t colours[NumBands] = {
            0xffffb703, // Band 1: Gold / Amber
            0xfffb8500, // Band 2: Vivid Orange
            0xffe63946, // Band 3: Crimson / Coral
            0xffd81159, // Band 4: Magenta / Rose
            0xff8f00ff, // Band 5: Electric Purple
            0xff00b4d8, // Band 6: Sky Blue / Cyan
            0xff06d6a0, // Band 7: Mint / Emerald
            0xff9ef01a  // Band 8: Lime Green
        };
        return juce::Colour(colours[bandIdx % NumBands]);
    }

    [[nodiscard]] static juce::String getFrequencyMusicalNote(float freqHz) noexcept {
        if (freqHz < 10.0f) return "";
        const float midiFloat = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
        const int midiNote = static_cast<int>(std::round(midiFloat));
        const float cents = (midiFloat - static_cast<float>(midiNote)) * 100.0f;

        static constexpr const char* NoteNames[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int noteIdx = ((midiNote % 12) + 12) % 12;
        const int octave = (midiNote / 12) - 1;

        juce::String str = NoteNames[noteIdx] + juce::String(octave);
        const int centsInt = static_cast<int>(std::round(cents));
        if (centsInt != 0) {
            str += (centsInt > 0 ? " +" : " ") + juce::String(centsInt) + "c";
        }
        return str;
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // 1. Sleek dark background
        g.fillAll(juce::Colour(0xff0e1117));

        // 2. Logarithmic frequency and decibel grid lines with labels & piano marks
        drawGrid(g, bounds);

        // 3. Real-time FFT Spectrum Analyzer Overlay
        if (hasSpectrum && spectrumBins > 0) {
            drawSpectrum(g, bounds);
        }

        // 4. Individual response curve for selected/hovered band
        drawIndividualCurves(g, bounds);

        // 5. Composite 8-band EQ Filter Response Curve
        drawCompositeCurve(g, bounds);

        // 6. Per-band dynamic range & real-time gain offset indicators
        drawDynamicIndicators(g, bounds);

        // 7. Draggable 8-Band Filter Nodes
        drawNodeHandles(g, bounds);

        // 8. Floating Parameter Readout Tooltip (Pro-Q 3 / ZL Style)
        drawFloatingBadge(g, bounds);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();
        int newHover = -1;
        float minDist = 20.0f;

        for (size_t i = 0; i < NumBands; ++i) {
            const auto pt = bandToScreen(bands[i], bounds);
            const float dist = e.position.getDistanceFrom(pt);
            if (dist < minDist) {
                minDist = dist;
                newHover = static_cast<int>(i);
            }
        }

        if (newHover != hoveredBand) {
            hoveredBand = newHover;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredBand != -1) {
            hoveredBand = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();
        int hitBand = -1;
        float minDist = 20.0f;

        for (size_t i = 0; i < NumBands; ++i) {
            const auto pt = bandToScreen(bands[i], bounds);
            const float dist = e.position.getDistanceFrom(pt);
            if (dist < minDist) {
                minDist = dist;
                hitBand = static_cast<int>(i);
            }
        }

        if (hitBand >= 0) {
            selectedBand = static_cast<size_t>(hitBand);
            draggingBand = hitBand;
            dragStartPos = e.position;
            dragStartGain = bands[selectedBand].gainDb;
            dragStartDynGain = bands[selectedBand].dynamicGainDb;
            isDraggingDynamics = e.mods.isAltDown();

            // Double-click resets gain or toggles bypass
            if (e.getNumberOfClicks() >= 2) {
                if (isDraggingDynamics) {
                    bands[selectedBand].dynamicGainDb = 0.0f;
                    bands[selectedBand].isDynamic = false;
                    if (onBandDynamicsChanged) {
                        onBandDynamicsChanged(selectedBand, 0.0f, bands[selectedBand].dynamicThresholdDb);
                    }
                } else if (std::abs(bands[selectedBand].gainDb) > 0.1f) {
                    bands[selectedBand].gainDb = 0.0f;
                    if (onBandChanged) {
                        onBandChanged(selectedBand, bands[selectedBand].frequency, 0.0f, bands[selectedBand].q);
                    }
                } else {
                    bands[selectedBand].bypassed = !bands[selectedBand].bypassed;
                    if (onBandBypassToggled) {
                        onBandBypassToggled(selectedBand, bands[selectedBand].bypassed);
                    }
                }
            }

            if (onBandSelected) {
                onBandSelected(selectedBand);
            }
            repaint();
        } else {
            // Click outside existing nodes: find closest band along frequency axis
            float minFreqDist = 1e9f;
            size_t closestIdx = 0;
            const float clickFreq = screenToFrequency(e.position.x, bounds);
            const float clickGain = screenToGainDb(e.position.y, bounds);

            for (size_t i = 0; i < NumBands; ++i) {
                const float d = std::abs(std::log10(bands[i].frequency) - std::log10(clickFreq));
                if (d < minFreqDist) {
                    minFreqDist = d;
                    closestIdx = i;
                }
            }
            selectedBand = closestIdx;

            // Double-click on empty canvas moves closest band directly to clicked spot (FabFilter Pro-Q 3 behavior)
            if (e.getNumberOfClicks() >= 2) {
                bands[selectedBand].frequency = std::clamp(clickFreq, 20.0f, 20000.0f);
                if (filterTypeUsesGain(bands[selectedBand].type)) {
                    bands[selectedBand].gainDb = std::clamp(clickGain, -30.0f, 30.0f);
                }
                if (onBandChanged) {
                    onBandChanged(selectedBand, bands[selectedBand].frequency, bands[selectedBand].gainDb, bands[selectedBand].q);
                }
            }

            if (onBandSelected) {
                onBandSelected(selectedBand);
            }
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (draggingBand < 0 || draggingBand >= static_cast<int>(NumBands)) return;

        const auto bounds = getLocalBounds().toFloat();
        const size_t b = static_cast<size_t>(draggingBand);
        const bool fineTune = e.mods.isShiftDown();
        const float fineFactor = fineTune ? 0.2f : 1.0f;

        if (isDraggingDynamics) {
            // Alt + Drag: adjust Dynamic Gain (Pro-Q 3 dynamic range collar)
            const float deltaY = (dragStartPos.y - e.position.y) * fineFactor;
            const float newDynGain = std::clamp(dragStartDynGain + (deltaY / bounds.getHeight()) * 60.0f, -30.0f, 30.0f);
            bands[b].dynamicGainDb = newDynGain;
            bands[b].isDynamic = (std::abs(newDynGain) > 0.1f);

            if (onBandDynamicsChanged) {
                onBandDynamicsChanged(b, bands[b].dynamicGainDb, bands[b].dynamicThresholdDb);
            }
        } else {
            // Standard Drag: Frequency & Gain
            float newFreq = screenToFrequency(e.position.x, bounds);
            if (fineTune) {
                const float startFreq = bands[b].frequency;
                newFreq = startFreq * std::pow(newFreq / startFreq, 0.2f);
            }
            bands[b].frequency = std::clamp(newFreq, 20.0f, 20000.0f);

            if (filterTypeUsesGain(bands[b].type)) {
                float newGain = screenToGainDb(e.position.y, bounds);
                if (fineTune) {
                    newGain = dragStartGain + (newGain - dragStartGain) * 0.2f;
                }
                bands[b].gainDb = std::clamp(newGain, -30.0f, 30.0f);
            } else {
                bands[b].gainDb = 0.0f;
            }

            if (onBandChanged) {
                onBandChanged(b, bands[b].frequency, bands[b].gainDb, bands[b].q);
            }
            if (onSingleBandChanged && b == selectedBand) {
                onSingleBandChanged(bands[b].frequency, bands[b].gainDb, bands[b].q);
            }
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        draggingBand = -1;
        isDraggingDynamics = false;
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        const size_t targetBand = (hoveredBand >= 0 && hoveredBand < static_cast<int>(NumBands))
                                      ? static_cast<size_t>(hoveredBand)
                                      : selectedBand;

        const float currentQ = bands[targetBand].q;
        const float newQ = std::clamp(currentQ * (1.0f + wheel.deltaY * 0.25f), 0.1f, 18.0f);
        bands[targetBand].q = newQ;

        if (onBandChanged) {
            onBandChanged(targetBand, bands[targetBand].frequency, bands[targetBand].gainDb, bands[targetBand].q);
        }
        if (onSingleBandChanged && targetBand == selectedBand) {
            onSingleBandChanged(bands[targetBand].frequency, bands[targetBand].gainDb, bands[targetBand].q);
        }
        repaint();
    }

private:
    std::array<FilterBandState, NumBands> bands{};
    size_t selectedBand{0};
    int hoveredBand{-1};
    int draggingBand{-1};
    bool isDraggingDynamics{false};
    juce::Point<float> dragStartPos{};
    float dragStartGain{0.0f};
    float dragStartDynGain{0.0f};

    std::array<float, 1024> spectrumData{};
    bool hasSpectrum{false};
    size_t spectrumBins{0};
    float sr{48000.0f};

    [[nodiscard]] static float freqToX(float freq, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = 1.30103f; // log10(20)
        constexpr float maxLog = 4.30103f; // log10(20000)
        const float fLog = std::log10(std::clamp(freq, 20.0f, 20000.0f));
        const float norm = (fLog - minLog) / (maxLog - minLog);
        return bounds.getX() + norm * bounds.getWidth();
    }

    [[nodiscard]] static float screenToFrequency(float x, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = 1.30103f;
        constexpr float maxLog = 4.30103f;
        const float norm = std::clamp((x - bounds.getX()) / bounds.getWidth(), 0.0f, 1.0f);
        return std::pow(10.0f, minLog + norm * (maxLog - minLog));
    }

    [[nodiscard]] static float gainToY(float gainDb, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -30.0f;
        constexpr float maxDb = 30.0f;
        const float norm = (std::clamp(gainDb, minDb, maxDb) - minDb) / (maxDb - minDb);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    [[nodiscard]] static float screenToGainDb(float y, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -30.0f;
        constexpr float maxDb = 30.0f;
        const float norm = 1.0f - std::clamp((y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);
        return minDb + norm * (maxDb - minDb);
    }

    [[nodiscard]] static juce::Point<float> bandToScreen(const FilterBandState& band, const juce::Rectangle<float>& bounds) noexcept {
        const float effectiveGain = filterTypeUsesGain(band.type) ? band.gainDb : 0.0f;
        return { freqToX(band.frequency, bounds), gainToY(effectiveGain, bounds) };
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setColour(juce::Colour(0xff1a1e24));
        static constexpr float Frequencies[] = { 30.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 16000.0f };
        static constexpr const char* FreqLabels[] = { "30", "50", "100", "250", "500", "1k", "2.5k", "5k", "10k", "16k" };

        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));

        for (size_t i = 0; i < 10; ++i) {
            const float x = freqToX(Frequencies[i], bounds);
            g.setColour(juce::Colour(0xff1c212a));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom() - 14.0f);

            g.setColour(juce::Colour(0xff606a77));
            g.drawText(FreqLabels[i], static_cast<int>(x) - 15, static_cast<int>(bounds.getBottom()) - 26, 30, 14, juce::Justification::centred);
        }

        // Piano roll musical note octaves along bottom bar
        static constexpr float PianoFreqs[8] = { 32.7f, 65.4f, 130.8f, 261.6f, 523.25f, 1046.5f, 2093.0f, 4186.0f };
        static constexpr const char* PianoNotes[8] = { "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8" };
        g.setFont(juce::FontOptions(9.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff3f4753));
        for (size_t i = 0; i < 8; ++i) {
            const float x = freqToX(PianoFreqs[i], bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getBottom() - 12.0f, bounds.getBottom());
            g.drawText(PianoNotes[i], static_cast<int>(x) - 10, static_cast<int>(bounds.getBottom()) - 12, 20, 12, juce::Justification::centred);
        }

        static constexpr float Gains[] = { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f, 24.0f };
        static constexpr const char* GainLabels[] = { "-24", "-18", "-12", "-6", "0 dB", "+6", "+12", "+18", "+24" };

        for (size_t i = 0; i < 9; ++i) {
            const float gDb = Gains[i];
            const float y = gainToY(gDb, bounds);
            const bool isZero = (gDb == 0.0f);

            g.setColour(isZero ? juce::Colour(0xff363f4c) : juce::Colour(0xff181c22));
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());

            g.setColour(isZero ? juce::Colour(0xff9aa4b2) : juce::Colour(0xff555e6b));
            g.drawText(GainLabels[i], static_cast<int>(bounds.getX()) + 6, static_cast<int>(y) - 7, 34, 14, juce::Justification::left);
        }
    }

    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path spectrumPath;
        const float binWidth = sr / static_cast<float>(spectrumBins * 2);
        bool firstPoint = true;

        for (size_t i = 1; i < spectrumBins; ++i) {
            const float freq = static_cast<float>(i) * binWidth;
            if (freq < 20.0f || freq > 20000.0f) continue;

            const float x = freqToX(freq, bounds);
            const float norm = std::clamp((spectrumData[i] + 90.0f) / 90.0f, 0.0f, 1.0f);
            const float y = bounds.getBottom() - norm * bounds.getHeight();

            if (firstPoint) {
                spectrumPath.startNewSubPath(x, bounds.getBottom());
                spectrumPath.lineTo(x, y);
                firstPoint = false;
            } else {
                spectrumPath.lineTo(x, y);
            }
        }

        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.closeSubPath();

        juce::ColourGradient grad(juce::Colour(0x2200f0ff), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour(0x0200f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(spectrumPath);
        g.setColour(juce::Colour(0x3800f0ff));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }

    void drawIndividualCurves(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        const auto drawSingle = [&](size_t bIdx, float alpha) {
            if (bIdx >= NumBands || bands[bIdx].bypassed) return;
            const auto& band = bands[bIdx];
            juce::Path p;
            const int numPoints = static_cast<int>(bounds.getWidth());
            for (int i = 0; i < numPoints; ++i) {
                const float x = bounds.getX() + static_cast<float>(i);
                const float f = screenToFrequency(x, bounds);
                const float magDb = openx::dsp::DynamicBiquadEngine<float>::computeMagnitudeDb(
                    band.type, f, band.frequency, band.q, band.gainDb);
                const float y = gainToY(std::clamp(magDb, -30.0f, 30.0f), bounds);
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(getBandColour(bIdx).withAlpha(alpha));
            g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        if (hoveredBand >= 0 && static_cast<size_t>(hoveredBand) != selectedBand) {
            drawSingle(static_cast<size_t>(hoveredBand), 0.35f);
        }
        drawSingle(selectedBand, 0.65f);
    }

    void drawCompositeCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path curvePath;
        juce::Path fillPath;
        const int numPoints = static_cast<int>(bounds.getWidth());
        const float zeroY = gainToY(0.0f, bounds);

        const bool anySolo = std::any_of(bands.begin(), bands.end(), [](const auto& b) {
            return b.solo && !b.bypassed;
        });

        for (int i = 0; i < numPoints; ++i) {
            const float x = bounds.getX() + static_cast<float>(i);
            const float freq = screenToFrequency(x, bounds);

            float totalDb = 0.0f;
            for (size_t b = 0; b < NumBands; ++b) {
                if (bands[b].bypassed) continue;
                if (anySolo && !bands[b].solo) continue;

                totalDb += openx::dsp::DynamicBiquadEngine<float>::computeMagnitudeDb(
                    bands[b].type, freq, bands[b].frequency, bands[b].q, bands[b].gainDb);
            }

            const float y = gainToY(std::clamp(totalDb, -30.0f, 30.0f), bounds);

            if (i == 0) {
                curvePath.startNewSubPath(x, y);
                fillPath.startNewSubPath(x, zeroY);
                fillPath.lineTo(x, y);
            } else {
                curvePath.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }

        fillPath.lineTo(bounds.getRight(), zeroY);
        fillPath.closeSubPath();

        // Subtle shaded area under composite response
        juce::ColourGradient fillGrad(juce::Colour(0x1a00f0ff), bounds.getCentreX(), bounds.getY(),
                                      juce::Colour(0x0300f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        // Vibrant composite line
        g.setColour(juce::Colour(0xff00f0ff));
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawDynamicIndicators(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        for (size_t i = 0; i < NumBands; ++i) {
            const auto& band = bands[i];
            if (!band.isDynamic || band.bypassed) continue;

            const auto pt = bandToScreen(band, bounds);
            const float maxDynGain = band.gainDb + band.dynamicGainDb;
            const float maxDynY = gainToY(maxDynGain, bounds);

            const bool isDownward = (band.dynamicGainDb <= 0.0f);
            const juce::Colour dynColor = isDownward ? juce::Colour(0xffffab00) : juce::Colour(0xff00e5ff);

            // Dynamic range track
            g.setColour(dynColor.withAlpha(0.6f));
            g.drawVerticalLine(static_cast<int>(pt.x), std::min(pt.y, maxDynY), std::max(pt.y, maxDynY));
            g.drawHorizontalLine(static_cast<int>(maxDynY), pt.x - 6.0f, pt.x + 6.0f);

            // Dynamic collar / ring around the node handle (FabFilter Pro-Q 3 style)
            g.setColour(dynColor.withAlpha(0.8f));
            g.drawEllipse(pt.x - 13.0f, pt.y - 13.0f, 26.0f, 26.0f, 1.6f);

            // Real-time dynamic modulation indicator (animated bead flexing with DSP envelope)
            if (std::abs(band.dynamicOffsetDb) > 0.05f) {
                const float currentModGain = band.gainDb + band.dynamicOffsetDb;
                const float currentModY = gainToY(currentModGain, bounds);

                g.setColour(dynColor);
                g.fillEllipse(pt.x - 5.0f, currentModY - 5.0f, 10.0f, 10.0f);
                g.setColour(juce::Colour(0xffffffff));
                g.drawEllipse(pt.x - 5.0f, currentModY - 5.0f, 10.0f, 10.0f, 1.2f);
            }
        }
    }

    void drawNodeHandles(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));

        for (size_t i = 0; i < NumBands; ++i) {
            const auto& band = bands[i];
            const auto pt = bandToScreen(band, bounds);
            const bool isSelected = (i == selectedBand);
            const bool isHovered  = (static_cast<int>(i) == hoveredBand);
            const juce::Colour bandColor = getBandColour(i);

            // Outer highlight halo for selected/hovered nodes
            if (isSelected) {
                g.setColour(juce::Colour(0xffffffff));
                g.drawEllipse(pt.x - 14.0f, pt.y - 14.0f, 28.0f, 28.0f, 2.0f);
                g.setColour(bandColor.withAlpha(0.35f));
                g.fillEllipse(pt.x - 14.0f, pt.y - 14.0f, 28.0f, 28.0f);
            } else if (isHovered) {
                g.setColour(bandColor.withAlpha(0.6f));
                g.drawEllipse(pt.x - 12.0f, pt.y - 12.0f, 24.0f, 24.0f, 1.5f);
            }

            // Solo indicator ring
            if (band.solo && !band.bypassed) {
                g.setColour(juce::Colour(0xffffd600));
                g.drawEllipse(pt.x - 16.0f, pt.y - 16.0f, 32.0f, 32.0f, 1.8f);
            }

            // Node core badge
            const float radius = 10.0f;
            const float alpha = band.bypassed ? 0.35f : 1.0f;
            g.setColour(bandColor.withAlpha(alpha));
            g.fillEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);

            // Node border
            g.setColour(juce::Colour(0xffffffff).withAlpha(alpha));
            g.drawEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

            // Band number badge text
            g.setColour(juce::Colour(0xff121418));
            g.drawText(juce::String(static_cast<int>(i + 1)),
                       static_cast<int>(pt.x - radius), static_cast<int>(pt.y - radius),
                       static_cast<int>(radius * 2.0f), static_cast<int>(radius * 2.0f),
                       juce::Justification::centred);

            // Bypassed diagonal slash
            if (band.bypassed) {
                g.setColour(juce::Colour(0xffff1744));
                g.drawLine(pt.x - radius, pt.y + radius, pt.x + radius, pt.y - radius, 2.0f);
            }
        }
    }

    void drawFloatingBadge(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        const int activeIdx = (draggingBand >= 0) ? draggingBand : hoveredBand;
        if (activeIdx < 0 || activeIdx >= static_cast<int>(NumBands)) return;

        const size_t b = static_cast<size_t>(activeIdx);
        const auto& band = bands[b];
        const auto pt = bandToScreen(band, bounds);

        // Tooltip text formatting
        juce::String freqStr;
        if (band.frequency >= 1000.0f) {
            freqStr = juce::String(band.frequency * 0.001f, 2) + " kHz";
        } else {
            freqStr = juce::String(static_cast<int>(std::round(band.frequency))) + " Hz";
        }

        const juce::String noteStr = getFrequencyMusicalNote(band.frequency);
        juce::String gainStr;
        if (filterTypeUsesGain(band.type)) {
            gainStr = (band.gainDb >= 0.0f ? "+" : "") + juce::String(band.gainDb, 1) + " dB";
        } else {
            gainStr = "0.0 dB";
        }

        juce::String titleLine = "Band " + juce::String(static_cast<int>(b + 1)) + " • " + getFilterTypeName(band.type);
        juce::String subLine = freqStr + " (" + noteStr + ") | " + gainStr + " | Q: " + juce::String(band.q, 2);
        if (band.isDynamic) {
            subLine += juce::String(" | Dyn: ") + (band.dynamicGainDb >= 0.0f ? "+" : "") + juce::String(band.dynamicGainDb, 1) + " dB";
        }

        const float badgeW = 230.0f;
        const float badgeH = 40.0f;
        float badgeX = pt.x - badgeW * 0.5f;
        float badgeY = pt.y - 54.0f;

        // Keep inside bounds
        badgeX = std::clamp(badgeX, bounds.getX() + 8.0f, bounds.getRight() - badgeW - 8.0f);
        if (badgeY < bounds.getY() + 8.0f) {
            badgeY = pt.y + 22.0f;
        }

        const auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);

        // Glassmorphic dark card
        g.setColour(juce::Colour(0xeb12151c));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setColour(getBandColour(b).withAlpha(0.7f));
        g.drawRoundedRectangle(badgeRect, 6.0f, 1.2f);

        // Title text
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(getBandColour(b));
        g.drawText(titleLine, badgeRect.reduced(8, 2).removeFromTop(16), juce::Justification::centred);

        // Value text
        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xffe6edf3));
        g.drawText(subLine, badgeRect.reduced(6, 2).removeFromBottom(18), juce::Justification::centred);
    }
};

} // namespace openx::ui
