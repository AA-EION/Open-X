#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../dsp/spectrum_analyzer.hpp"
#include <functional>
#include <cmath>

namespace openx::ui {

class InteractiveEqCurve : public juce::Component {
public:
    struct FilterBandState {
        float frequency{1000.0f};
        float gainDb{0.0f};
        float q{0.7071f};
        float dynamicGainDb{0.0f};
        bool isDynamic{false};
    };

    std::function<void(float freq, float gainDb, float q)> onBandChanged;

    InteractiveEqCurve() = default;

    void setBandState(const FilterBandState& state) noexcept {
        bandState = state;
        repaint();
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

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff121418));

        // 1. Grid lines
        drawGrid(g, bounds);

        // 2. Real-time Spectrum Analyzer Overlay
        if (hasSpectrum && spectrumBins > 0) {
            drawSpectrum(g, bounds);
        }

        // 3. Composite EQ Filter Curve
        drawFilterCurve(g, bounds);

        // 4. Draggable Filter Node
        drawNodeHandle(g, bounds);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto nodePos = bandToScreen(bandState.frequency, bandState.gainDb, bounds);
        if (e.position.getDistanceFrom(nodePos) < 18.0f) {
            isDraggingNode = true;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!isDraggingNode) return;
        const auto bounds = getLocalBounds().toFloat();
        const float newFreq = screenToFrequency(e.position.x, bounds);
        const float newGain = screenToGainDb(e.position.y, bounds);

        bandState.frequency = std::clamp(newFreq, 20.0f, 20000.0f);
        bandState.gainDb = std::clamp(newGain, -24.0f, 24.0f);

        if (onBandChanged) onBandChanged(bandState.frequency, bandState.gainDb, bandState.q);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        isDraggingNode = false;
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        bandState.q = std::clamp(bandState.q + wheel.deltaY * 0.5f, 0.1f, 18.0f);
        if (onBandChanged) onBandChanged(bandState.frequency, bandState.gainDb, bandState.q);
        repaint();
    }

private:
    FilterBandState bandState{};
    std::array<float, 1024> spectrumData{};
    bool hasSpectrum{false};
    size_t spectrumBins{0};
    float sr{48000.0f};
    bool isDraggingNode{false};

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
        constexpr float minDb = -24.0f;
        constexpr float maxDb = 24.0f;
        const float norm = (std::clamp(gainDb, minDb, maxDb) - minDb) / (maxDb - minDb);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    [[nodiscard]] static float screenToGainDb(float y, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -24.0f;
        constexpr float maxDb = 24.0f;
        const float norm = 1.0f - std::clamp((y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);
        return minDb + norm * (maxDb - minDb);
    }

    [[nodiscard]] static juce::Point<float> bandToScreen(float freq, float gainDb, const juce::Rectangle<float>& bounds) noexcept {
        return { freqToX(freq, bounds), gainToY(gainDb, bounds) };
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setColour(juce::Colour(0xff22272e));
        static constexpr float Frequencies[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f };
        for (float f : Frequencies) {
            const float x = freqToX(f, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }

        static constexpr float Gains[] = { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f };
        for (float gDb : Gains) {
            const float y = gainToY(gDb, bounds);
            g.setColour(gDb == 0.0f ? juce::Colour(0xff3d444d) : juce::Colour(0xff22272e));
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
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
            // Map [-90 dB, 0 dB] -> screen Y
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

        // Gradient spectral fill
        juce::ColourGradient grad(juce::Colour(0x2200f0ff), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour(0x0200f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(spectrumPath);
        g.setColour(juce::Colour(0x5500f0ff));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }

    void drawFilterCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path curvePath;
        const int numPoints = static_cast<int>(bounds.getWidth());
        for (int i = 0; i < numPoints; ++i) {
            const float x = bounds.getX() + static_cast<float>(i);
            const float freq = screenToFrequency(x, bounds);

            // Biquad peaking filter magnitude response equation:
            // |H(w)|^2 = ( (1 - w^2)^2 + (w / Q)^2 * G^2 ) / ( (1 - w^2)^2 + (w / Q)^2 )
            const float w = freq / std::max(bandState.frequency, 1.0f);
            const float w2 = w * w;
            const float oneMinusW2Sq = (1.0f - w2) * (1.0f - w2);
            const float linGain = std::pow(10.0f, bandState.gainDb / 20.0f);
            const float qTerm = (w / std::max(bandState.q, 0.05f)) * (w / std::max(bandState.q, 0.05f));

            const float magSq = (oneMinusW2Sq + qTerm * linGain * linGain) / (oneMinusW2Sq + qTerm);
            const float magDb = 10.0f * std::log10(std::max(magSq, 1e-6f));
            const float y = gainToY(magDb, bounds);

            if (i == 0) curvePath.startNewSubPath(x, y);
            else curvePath.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xff00f0ff));
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawNodeHandle(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        const auto pt = bandToScreen(bandState.frequency, bandState.gainDb, bounds);

        // Dynamic range modulation ring
        if (bandState.isDynamic) {
            const auto dynPt = bandToScreen(bandState.frequency, bandState.gainDb + bandState.dynamicGainDb, bounds);
            g.setColour(juce::Colour(0x66ffab00));
            g.drawVerticalLine(static_cast<int>(pt.x), std::min(pt.y, dynPt.y), std::max(pt.y, dynPt.y));
            g.drawEllipse(dynPt.x - 5.0f, dynPt.y - 5.0f, 10.0f, 10.0f, 1.5f);
        }

        // Main node handle
        g.setColour(juce::Colour(0xff00f0ff));
        g.fillEllipse(pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);
        g.setColour(juce::Colour(0xffffffff));
        g.drawEllipse(pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, 1.5f);
    }
};

} // namespace openx::ui
