#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../dsp/spectrum_analyzer.hpp"
#include <functional>
#include <cmath>
#include <array>
#include <algorithm>

namespace openx::ui {

class DeEsserVisualizer : public juce::Component {
public:
    struct FilterBand {
        float frequency{6000.0f};
        float thresholdDb{-24.0f};
        float q{2.0f};
        bool isSplitBand{true};
        bool isHighpass{false};
    };

    std::function<void(float freq, float thresholdDb, float q)> onBandChanged;
    std::function<void()> onGestureStarted;
    std::function<void()> onGestureEnded;

    [[nodiscard]] bool isDragging() const noexcept { return isDraggingNode; }

    DeEsserVisualizer() = default;

    void setFilterBand(float freq, float thresholdDb, float q, bool isSplitBand, bool isHighpass) noexcept {
        band.frequency = std::clamp(freq, 1000.0f, 18000.0f);
        band.thresholdDb = std::clamp(thresholdDb, -60.0f, 0.0f);
        band.q = std::clamp(q, 0.5f, 8.0f);
        band.isSplitBand = isSplitBand;
        band.isHighpass = isHighpass;
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
        sr = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
        repaint();
    }

    void updateDynamics(float gainReductionDb, float sibilanceActivity, float sidechainLevelDb) noexcept {
        currentGrDb = gainReductionDb;
        currentSibilance = std::clamp(sibilanceActivity, 0.0f, 1.0f);
        currentScLevelDb = sidechainLevelDb;

        // Smooth visual meters
        smoothGrDb = smoothGrDb * 0.70f + currentGrDb * 0.30f;
        smoothSibilance = smoothSibilance * 0.65f + currentSibilance * 0.35f;

        // Peak hold on gain reduction
        if (currentGrDb < peakGrDb) {
            peakGrDb = currentGrDb;
            peakHoldCounter = 40; // ~650 ms hold at 60 Hz
        } else {
            if (peakHoldCounter > 0) {
                --peakHoldCounter;
            } else {
                peakGrDb = peakGrDb * 0.94f + currentGrDb * 0.06f;
            }
        }

        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto totalBounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff121418)); // OpenX background dark

        // Layout: Main spectrum on the left, Gain Reduction meter on the right
        constexpr float meterWidth = 52.0f;
        const auto spectrumBounds = totalBounds.withTrimmedRight(meterWidth).reduced(4.0f);
        const auto meterBounds = totalBounds.withLeft(spectrumBounds.getRight() + 4.0f).reduced(2.0f);

        // 1. Grid lines and frequency / dB markings
        drawGrid(g, spectrumBounds);

        // 2. Active Sidechain Detection Band shading
        drawDetectionBandZone(g, spectrumBounds);

        // 3. Main Real-time Audio Spectrum (Cyan)
        if (hasSpectrum && spectrumBins > 0) {
            drawSpectrum(g, spectrumBounds);
        }

        // 4. Highlighted Sibilance Energy Overlay (Yellow/Amber/Orange)
        if (hasSpectrum && spectrumBins > 0 && (smoothSibilance > 0.015f || smoothGrDb < -0.1f)) {
            drawSibilanceHighlight(g, spectrumBounds);
        }

        // 5. Sidechain Filter Band Response Curve
        drawFilterCurve(g, spectrumBounds);

        // 6. Interactive Filter & Threshold Node Handle
        drawNodeHandle(g, spectrumBounds);

        // 7. Gain Reduction Meter on the right
        drawGainReductionMeter(g, meterBounds);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto totalBounds = getLocalBounds().toFloat();
        constexpr float meterWidth = 52.0f;
        const auto spectrumBounds = totalBounds.withTrimmedRight(meterWidth).reduced(4.0f);

        const auto nodePos = bandToScreen(band.frequency, band.thresholdDb, spectrumBounds);
        if (e.position.getDistanceFrom(nodePos) < 22.0f) {
            isDraggingNode = true;
            if (onGestureStarted) onGestureStarted();
        } else if (spectrumBounds.contains(e.position)) {
            // Click inside spectrum: move node to click position
            const float newFreq = screenToFrequency(e.position.x, spectrumBounds);
            const float newThresh = screenToDb(e.position.y, spectrumBounds);
            band.frequency = std::clamp(newFreq, 1000.0f, 18000.0f);
            band.thresholdDb = std::clamp(newThresh, -60.0f, 0.0f);
            isDraggingNode = true;
            if (onGestureStarted) onGestureStarted();
            if (onBandChanged) onBandChanged(band.frequency, band.thresholdDb, band.q);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!isDraggingNode) return;
        const auto totalBounds = getLocalBounds().toFloat();
        constexpr float meterWidth = 52.0f;
        const auto spectrumBounds = totalBounds.withTrimmedRight(meterWidth).reduced(4.0f);

        const float newFreq = screenToFrequency(e.position.x, spectrumBounds);
        const float newThresh = screenToDb(e.position.y, spectrumBounds);

        band.frequency = std::clamp(newFreq, 1000.0f, 18000.0f);
        band.thresholdDb = std::clamp(newThresh, -60.0f, 0.0f);

        if (onBandChanged) onBandChanged(band.frequency, band.thresholdDb, band.q);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (isDraggingNode) {
            isDraggingNode = false;
            if (onGestureEnded) onGestureEnded();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        if (onGestureStarted) onGestureStarted();
        band.q = std::clamp(band.q + wheel.deltaY * 0.4f, 0.5f, 8.0f);
        if (onBandChanged) onBandChanged(band.frequency, band.thresholdDb, band.q);
        if (onGestureEnded) onGestureEnded();
        repaint();
    }

private:
    FilterBand band{};
    std::array<float, 1024> spectrumData{};
    bool hasSpectrum{false};
    size_t spectrumBins{0};
    float sr{48000.0f};

    float currentGrDb{0.0f};
    float smoothGrDb{0.0f};
    float peakGrDb{0.0f};
    int peakHoldCounter{0};
    float currentSibilance{0.0f};
    float smoothSibilance{0.0f};
    float currentScLevelDb{-100.0f};

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

    [[nodiscard]] static float dbToY(float db, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -72.0f;
        constexpr float maxDb = 6.0f;
        const float norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    [[nodiscard]] static float screenToDb(float y, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -72.0f;
        constexpr float maxDb = 6.0f;
        const float norm = 1.0f - std::clamp((y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);
        return minDb + norm * (maxDb - minDb);
    }

    [[nodiscard]] static juce::Point<float> bandToScreen(float freq, float db, const juce::Rectangle<float>& bounds) noexcept {
        return { freqToX(freq, bounds), dbToY(db, bounds) };
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setColour(juce::Colour(0xff22272e));

        static constexpr float Frequencies[] = { 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
        for (float f : Frequencies) {
            const float x = freqToX(f, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            g.setColour(juce::Colour(0xff6e7681));
            g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
            juce::String label = (f >= 1000.0f) ? juce::String(static_cast<int>(f / 1000.0f)) + "k" : juce::String(static_cast<int>(f));
            g.drawText(label, static_cast<int>(x) - 15, static_cast<int>(bounds.getBottom()) - 16, 30, 14, juce::Justification::centred);
            g.setColour(juce::Colour(0xff22272e));
        }

        static constexpr float Dbs[] = { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f };
        for (float db : Dbs) {
            const float y = dbToY(db, bounds);
            g.setColour(db == 0.0f ? juce::Colour(0xff3d444d) : juce::Colour(0xff22272e));
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());

            g.setColour(juce::Colour(0xff6e7681));
            g.setFont(juce::FontOptions(9.0f, juce::Font::plain));
            g.drawText(juce::String(static_cast<int>(db)) + " dB", static_cast<int>(bounds.getX()) + 4, static_cast<int>(y) - 12, 35, 12, juce::Justification::left);
        }
    }

    void drawDetectionBandZone(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        float fLow = band.frequency;
        float fHigh = 20000.0f;

        if (band.isHighpass) {
            fLow = band.frequency;
            fHigh = 20000.0f;
        } else {
            // Bandpass bandwidth based on Q
            const float halfBwFactor = std::pow(2.0f, 0.5f / std::max(band.q, 0.1f));
            fLow = band.frequency / halfBwFactor;
            fHigh = band.frequency * halfBwFactor;
        }

        const float x1 = freqToX(std::max(fLow, 20.0f), bounds);
        const float x2 = freqToX(std::min(fHigh, 20000.0f), bounds);
        const float bandW = std::max(x2 - x1, 4.0f);

        // Soft amber shaded zone
        juce::ColourGradient bandGrad(juce::Colour(0x18ffab00), x1 + bandW * 0.5f, bounds.getY(),
                                     juce::Colour(0x04ffab00), x1 + bandW * 0.5f, bounds.getBottom(), false);
        g.setGradientFill(bandGrad);
        g.fillRect(x1, bounds.getY(), bandW, bounds.getHeight());

        // Vertical boundary lines
        g.setColour(juce::Colour(0x33ffab00));
        g.drawVerticalLine(static_cast<int>(x1), bounds.getY(), bounds.getBottom());
        if (!band.isHighpass) {
            g.drawVerticalLine(static_cast<int>(x2), bounds.getY(), bounds.getBottom());
        }

        // Horizontal threshold reference line across the active band
        const float threshY = dbToY(band.thresholdDb, bounds);
        g.setColour(juce::Colour(0x77ffab00));
        static constexpr float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(x1, threshY, x2, threshY), dashes, 2, 1.2f);
    }

    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path spectrumLinePath;
        const float binWidth = sr / static_cast<float>(spectrumBins * 2);
        bool firstPoint = true;
        float firstX = bounds.getX();
        float lastX = bounds.getRight();

        for (size_t i = 1; i < spectrumBins; ++i) {
            const float freq = static_cast<float>(i) * binWidth;
            if (freq < 20.0f || freq > 20000.0f) continue;

            const float x = freqToX(freq, bounds);
            const float y = dbToY(spectrumData[i], bounds);

            if (firstPoint) {
                firstX = x;
                spectrumLinePath.startNewSubPath(x, y);
                firstPoint = false;
            } else {
                spectrumLinePath.lineTo(x, y);
            }
            lastX = x;
        }

        if (!firstPoint) {
            juce::Path spectrumFillPath = spectrumLinePath;
            spectrumFillPath.lineTo(lastX, bounds.getBottom());
            spectrumFillPath.lineTo(firstX, bounds.getBottom());
            spectrumFillPath.closeSubPath();

            // Spectral fill (Cool Cyan)
            juce::ColourGradient grad(juce::Colour(0x2200f0ff), bounds.getCentreX(), bounds.getY(),
                                      juce::Colour(0x0200f0ff), bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(spectrumFillPath);

            // Cyan stroke line (only along the curve)
            g.setColour(juce::Colour(0x8800f0ff));
            g.strokePath(spectrumLinePath, juce::PathStrokeType(1.2f));
        }
    }

    void drawSibilanceHighlight(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        float fLow = band.frequency;
        float fHigh = 20000.0f;
        if (band.isHighpass) {
            fLow = band.frequency;
            fHigh = 20000.0f;
        } else {
            const float halfBwFactor = std::pow(2.0f, 0.5f / std::max(band.q, 0.1f));
            fLow = band.frequency / halfBwFactor;
            fHigh = band.frequency * halfBwFactor;
        }

        juce::Path sibLinePath;
        const float binWidth = sr / static_cast<float>(spectrumBins * 2);
        bool firstPoint = true;
        float startX = 0;
        float lastX = 0;

        for (size_t i = 1; i < spectrumBins; ++i) {
            const float freq = static_cast<float>(i) * binWidth;
            if (freq < fLow || freq > fHigh) continue;

            const float x = freqToX(freq, bounds);
            const float y = dbToY(spectrumData[i], bounds);

            if (firstPoint) {
                startX = x;
                sibLinePath.startNewSubPath(x, y);
                firstPoint = false;
            } else {
                sibLinePath.lineTo(x, y);
            }
            lastX = x;
        }

        if (!firstPoint) {
            juce::Path sibFillPath = sibLinePath;
            sibFillPath.lineTo(lastX, bounds.getBottom());
            sibFillPath.lineTo(startX, bounds.getBottom());
            sibFillPath.closeSubPath();

            // Calculate glowing amber/orange intensity from sibilance & GR
            const float intensity = std::clamp(smoothSibilance * 1.6f + (-smoothGrDb / 18.0f), 0.2f, 1.0f);
            const uint8_t alphaTop = static_cast<uint8_t>(220.0f * intensity);
            const uint8_t alphaBottom = static_cast<uint8_t>(30.0f * intensity);

            // Sibilance highlight fill in golden amber / orange
            juce::ColourGradient sibGrad(juce::Colour::fromRGBA(0xff, 0xab, 0x00, alphaTop), (startX + lastX) * 0.5f, bounds.getY(),
                                         juce::Colour::fromRGBA(0xff, 0x6d, 0x00, alphaBottom), (startX + lastX) * 0.5f, bounds.getBottom(), false);
            g.setGradientFill(sibGrad);
            g.fillPath(sibFillPath);

            // Radiant amber stroke line (only along the curve)
            g.setColour(juce::Colour::fromRGBA(0xff, 0xc1, 0x07, static_cast<uint8_t>(255.0f * intensity)));
            g.strokePath(sibLinePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawFilterCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path curvePath;
        const int numPoints = static_cast<int>(bounds.getWidth());

        for (int i = 0; i < numPoints; ++i) {
            const float x = bounds.getX() + static_cast<float>(i);
            const float freq = screenToFrequency(x, bounds);

            float magDb = 0.0f;
            if (band.isHighpass) {
                // Highpass sidechain response approximation
                const float ratio = freq / std::max(band.frequency, 20.0f);
                magDb = 10.0f * std::log10(std::max(std::pow(ratio, 4.0f) / (1.0f + std::pow(ratio, 4.0f)), 1e-6f));
                magDb = std::clamp(magDb, -48.0f, 0.0f);
            } else {
                // Bandpass bell response
                const float w = freq / std::max(band.frequency, 20.0f);
                const float qTerm = (w / std::max(band.q, 0.1f));
                const float denom = (1.0f - w * w) * (1.0f - w * w) + qTerm * qTerm;
                const float magSq = (qTerm * qTerm) / std::max(denom, 1e-6f);
                magDb = 10.0f * std::log10(std::max(magSq, 1e-6f));
                magDb = std::clamp(magDb, -48.0f, 0.0f);
            }

            // Offset to threshold level
            const float displayDb = band.thresholdDb + magDb;
            const float y = dbToY(displayDb, bounds);

            if (i == 0) curvePath.startNewSubPath(x, y);
            else curvePath.lineTo(x, y);
        }

        g.setColour(juce::Colour(0x99ffab00));
        static constexpr float dashes[] = { 3.0f, 3.0f };
        g.strokePath(curvePath, juce::PathStrokeType(1.5f));
    }

    void drawNodeHandle(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        const auto pt = bandToScreen(band.frequency, band.thresholdDb, bounds);

        // Outer glow
        g.setColour(juce::Colour(0x44ffab00));
        g.fillEllipse(pt.x - 12.0f, pt.y - 12.0f, 24.0f, 24.0f);

        // Core handle
        g.setColour(juce::Colour(0xffffab00));
        g.fillEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colour(0xffffffff));
        g.drawEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f, 1.5f);

        // Info readout badge
        juce::String infoText = juce::String(static_cast<int>(band.frequency)) + " Hz | "
                              + juce::String(band.thresholdDb, 1) + " dB | Q "
                              + juce::String(band.q, 2);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        const int textW = 140;
        const int textH = 18;
        const float badgeX = std::clamp(pt.x - textW * 0.5f, bounds.getX() + 4.0f, bounds.getRight() - textW - 4.0f);
        const float badgeY = std::max(pt.y - 28.0f, bounds.getY() + 4.0f);

        g.setColour(juce::Colour(0xee1c2128));
        g.fillRoundedRectangle(badgeX, badgeY, textW, textH, 4.0f);
        g.setColour(juce::Colour(0xff3d444d));
        g.drawRoundedRectangle(badgeX, badgeY, textW, textH, 4.0f, 1.0f);

        g.setColour(juce::Colour(0xffffab00));
        g.drawText(infoText, static_cast<int>(badgeX), static_cast<int>(badgeY), textW, textH, juce::Justification::centred);
    }

    void drawGainReductionMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        // Channel background
        g.setColour(juce::Colour(0xff161a22));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff2d333b));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Header label "GR"
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("GR", bounds.getX(), bounds.getY() + 3.0f, bounds.getWidth(), 14.0f, juce::Justification::centred);

        const auto meterBarArea = bounds.reduced(6.0f, 22.0f);

        // Tick marks: 0, -6, -12, -18, -24
        static constexpr float GrTicks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
        g.setFont(juce::FontOptions(9.0f, juce::Font::plain));

        for (float tickDb : GrTicks) {
            const float norm = -tickDb / 24.0f;
            const float tickY = meterBarArea.getY() + norm * meterBarArea.getHeight();

            g.setColour(juce::Colour(0xff3d444d));
            g.drawHorizontalLine(static_cast<int>(tickY), meterBarArea.getX() + 2.0f, meterBarArea.getX() + 8.0f);

            g.setColour(juce::Colour(0xff6e7681));
            g.drawText(juce::String(static_cast<int>(-tickDb)), meterBarArea.getX() + 10.0f, static_cast<int>(tickY) - 6, 26, 12, juce::Justification::left);
        }

        // Active Gain Reduction Bar (extends downwards from top 0 dB)
        const float grClamped = std::clamp(-smoothGrDb, 0.0f, 24.0f);
        const float grBarH = (grClamped / 24.0f) * meterBarArea.getHeight();

        if (grBarH > 1.0f) {
            const auto barRect = juce::Rectangle<float>(meterBarArea.getX() + 34.0f, meterBarArea.getY(), 10.0f, grBarH);
            juce::ColourGradient grGrad(juce::Colour(0xffffab00), barRect.getCentreX(), meterBarArea.getY(),
                                       juce::Colour(0xffff1744), barRect.getCentreX(), meterBarArea.getY() + meterBarArea.getHeight(), false);
            g.setGradientFill(grGrad);
            g.fillRoundedRectangle(barRect, 2.0f);
        }

        // Peak hold tick line
        const float peakClamped = std::clamp(-peakGrDb, 0.0f, 24.0f);
        if (peakClamped > 0.2f) {
            const float peakY = meterBarArea.getY() + (peakClamped / 24.0f) * meterBarArea.getHeight();
            g.setColour(juce::Colour(0xffffd600));
            g.drawHorizontalLine(static_cast<int>(peakY), meterBarArea.getX() + 32.0f, meterBarArea.getX() + 46.0f);
        }

        // Current dB readout text at bottom
        g.setColour(smoothGrDb < -0.1f ? juce::Colour(0xffffab00) : juce::Colour(0xff8b949e));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(juce::String(smoothGrDb, 1) + " dB", bounds.getX(), bounds.getBottom() - 18.0f, bounds.getWidth(), 14.0f, juce::Justification::centred);
    }
};

} // namespace openx::ui
