#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../dsp/spectrum_analyzer.hpp"
#include <functional>
#include <cmath>
#include <array>
#include <algorithm>
#include <string>

namespace openx::ui {

class MultibandCurve : public juce::Component {
public:
    static constexpr size_t NumBands = 4;
    static constexpr size_t NumCrossovers = NumBands - 1;

    struct BandState {
        float thresholdDb{-18.0f};
        float rangeDb{-12.0f};
        float makeupGainDb{0.0f};
        float currentGainChangeDb{0.0f};
        int mode{0}; // 0 = Compress, 1 = Expand
        bool solo{false};
        bool mute{false};
        bool bypass{false};
        juce::Colour colour{juce::Colour(0xff00e5ff)};
        juce::String name{"Band"};
    };

    std::function<void(size_t bandIndex)> onBandSelected;
    std::function<void(size_t crossoverIndex, float newFreq)> onCrossoverChanged;
    std::function<void(size_t bandIndex, float newGainDb)> onBandGainChanged;

    MultibandCurve() {
        setOpaque(true);

        bands[0].colour = juce::Colour(0xffff5252); // Low: Coral Red
        bands[0].name = "LOW";
        bands[1].colour = juce::Colour(0xffffb74d); // Low-Mid: Warm Amber
        bands[1].name = "LOW-MID";
        bands[2].colour = juce::Colour(0xff00e5ff); // High-Mid: Electric Cyan
        bands[2].name = "HIGH-MID";
        bands[3].colour = juce::Colour(0xffb388ff); // High: Neon Violet
        bands[3].name = "HIGH";

        crossovers[0] = 160.0f;
        crossovers[1] = 1200.0f;
        crossovers[2] = 6000.0f;
    }

    void setBandState(size_t index, const BandState& state) noexcept {
        if (index < NumBands) {
            if (draggingBandNode == static_cast<int>(index)) {
                const float savedGain = bands[index].makeupGainDb;
                bands[index] = state;
                bands[index].makeupGainDb = savedGain;
            } else {
                bands[index] = state;
            }
            repaint();
        }
    }

    void setSelectedBand(size_t index) noexcept {
        if (index < NumBands && selectedBand != index) {
            selectedBand = index;
            repaint();
        }
    }

    [[nodiscard]] size_t getSelectedBand() const noexcept {
        return selectedBand;
    }

    void setCrossoverFrequency(size_t index, float freq) noexcept {
        if (index < NumCrossovers) {
            if (draggingCrossover == static_cast<int>(index)) return;
            crossovers[index] = freq;
            repaint();
        }
    }

    [[nodiscard]] float getCrossoverFrequency(size_t index) const noexcept {
        if (index < NumCrossovers) {
            return crossovers[index];
        }
        return 1000.0f;
    }

    void setBandGainChange(size_t index, float gainChangeDb) noexcept {
        if (index < NumBands) {
            bands[index].currentGainChangeDb = gainChangeDb;
        }
    }

    void updateSpectrum(const float* scopeData, size_t numBins, float sampleRate) noexcept {
        if (scopeData != nullptr && numBins > 0) {
            spectrumBins = std::min(numBins, spectrumData.size());
            std::copy_n(scopeData, spectrumBins, spectrumData.begin());
            hasSpectrum = true;
        } else {
            hasSpectrum = false;
        }
        sr = sampleRate > 0 ? sampleRate : 48000.0f;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // 1. Dark background
        g.fillAll(juce::Colour(0xff12151b));

        // 2. Frequency and Gain Grid lines
        drawGrid(g, bounds);

        // 3. FFT Real-Time Spectrum Overlay
        if (hasSpectrum && spectrumBins > 0) {
            drawSpectrum(g, bounds);
        }

        // 4. Per-band frequency regions and dynamic ribbons
        drawBandRegions(g, bounds);

        // 5. Composite dynamic curve
        drawCompositeCurve(g, bounds);

        // 6. Interactive handles and crossover dividers
        drawBandHandles(g, bounds);
        drawCrossoverDividers(g, bounds);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();
        hoveredCrossover = -1;
        hoveredBandNode = -1;

        // Check crossover dividers (badge or vertical line)
        for (size_t i = 0; i < NumCrossovers; ++i) {
            const float x = freqToX(crossovers[i], bounds);
            const bool hitBadge = (e.position.y < bounds.getY() + 26.0f && std::abs(e.position.x - x) < 24.0f);
            const bool hitLine = (std::abs(e.position.x - x) < 8.0f);
            if (hitBadge || hitLine) {
                hoveredCrossover = static_cast<int>(i);
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                repaint();
                return;
            }
        }

        // Check band center nodes
        for (size_t b = 0; b < NumBands; ++b) {
            const auto nodePt = getBandCenterPoint(b, bounds);
            if (e.position.getDistanceFrom(nodePt) < 14.0f) {
                hoveredBandNode = static_cast<int>(b);
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                repaint();
                return;
            }
        }

        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();

        // Check crossover divider handle hit (badge or vertical line)
        for (size_t i = 0; i < NumCrossovers; ++i) {
            const float x = freqToX(crossovers[i], bounds);
            const bool hitBadge = (e.position.y < bounds.getY() + 26.0f && std::abs(e.position.x - x) < 24.0f);
            const bool hitLine = (std::abs(e.position.x - x) < 10.0f);
            if (hitBadge || hitLine) {
                draggingCrossover = static_cast<int>(i);
                repaint();
                return;
            }
        }

        // Check band center node hit
        for (size_t b = 0; b < NumBands; ++b) {
            const auto nodePt = getBandCenterPoint(b, bounds);
            if (e.position.getDistanceFrom(nodePt) < 14.0f) {
                draggingBandNode = static_cast<int>(b);
                selectedBand = b;
                if (onBandSelected) onBandSelected(selectedBand);
                repaint();
                return;
            }
        }

        // Click inside band region -> select band
        const float clickedFreq = screenToFreq(e.position.x, bounds);
        const size_t bandIdx = getBandForFrequency(clickedFreq);
        if (bandIdx < NumBands) {
            selectedBand = bandIdx;
            if (onBandSelected) onBandSelected(selectedBand);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();

        if (draggingCrossover >= 0 && draggingCrossover < static_cast<int>(NumCrossovers)) {
            const size_t idx = static_cast<size_t>(draggingCrossover);
            const float newFreq = screenToFreq(e.position.x, bounds);

            float minF = 25.0f;
            float maxF = 19000.0f;

            if (idx == 0) {
                minF = 25.0f;
                maxF = crossovers[1] * 0.85f;
            } else if (idx == 1) {
                minF = crossovers[0] * 1.15f;
                maxF = crossovers[2] * 0.85f;
            } else if (idx == 2) {
                minF = crossovers[1] * 1.15f;
                maxF = 19000.0f;
            }

            // Guard against inverted bounds to prevent std::clamp assertion failure
            if (minF > maxF) {
                const float mid = 0.5f * (minF + maxF);
                minF = mid * 0.95f;
                maxF = mid * 1.05f;
            }

            crossovers[idx] = std::clamp(newFreq, minF, maxF);
            if (onCrossoverChanged) onCrossoverChanged(idx, crossovers[idx]);
            repaint();
            return;
        }

        if (draggingBandNode >= 0 && draggingBandNode < static_cast<int>(NumBands)) {
            const size_t b = static_cast<size_t>(draggingBandNode);
            const float newGain = screenToGain(e.position.y, bounds);
            bands[b].makeupGainDb = std::clamp(newGain, -24.0f, 24.0f);
            if (onBandGainChanged) onBandGainChanged(b, bands[b].makeupGainDb);
            repaint();
            return;
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        draggingCrossover = -1;
        draggingBandNode = -1;
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();

        // Check crossover double-click reset
        for (size_t i = 0; i < NumCrossovers; ++i) {
            const float x = freqToX(crossovers[i], bounds);
            if (std::abs(e.position.x - x) < 10.0f) {
                static constexpr float DefaultFreqs[3] = { 160.0f, 1200.0f, 6000.0f };
                crossovers[i] = DefaultFreqs[i];
                if (onCrossoverChanged) onCrossoverChanged(i, crossovers[i]);
                repaint();
                return;
            }
        }

        // Check band center node reset
        for (size_t b = 0; b < NumBands; ++b) {
            const auto nodePt = getBandCenterPoint(b, bounds);
            if (e.position.getDistanceFrom(nodePt) < 14.0f) {
                bands[b].makeupGainDb = 0.0f;
                if (onBandGainChanged) onBandGainChanged(b, 0.0f);
                repaint();
                return;
            }
        }
    }

private:
    std::array<BandState, NumBands> bands{};
    std::array<float, NumCrossovers> crossovers{ 160.0f, 1200.0f, 6000.0f };
    std::array<float, 1024> spectrumData{};
    bool hasSpectrum{false};
    size_t spectrumBins{0};
    float sr{48000.0f};

    size_t selectedBand{0};
    int hoveredCrossover{-1};
    int draggingCrossover{-1};
    int hoveredBandNode{-1};
    int draggingBandNode{-1};

    [[nodiscard]] static float freqToX(float freq, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = 1.30103f; // log10(20)
        constexpr float maxLog = 4.30103f; // log10(20000)
        const float fLog = std::log10(std::clamp(freq, 20.0f, 20000.0f));
        const float norm = (fLog - minLog) / (maxLog - minLog);
        return bounds.getX() + norm * bounds.getWidth();
    }

    [[nodiscard]] static float screenToFreq(float x, const juce::Rectangle<float>& bounds) noexcept {
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

    [[nodiscard]] static float screenToGain(float y, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -24.0f;
        constexpr float maxDb = 24.0f;
        const float norm = 1.0f - std::clamp((y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);
        return minDb + norm * (maxDb - minDb);
    }

    [[nodiscard]] size_t getBandForFrequency(float freq) const noexcept {
        const auto [f0_0, f0_1] = getBandFreqRange(0);
        const auto [f1_0, f1_1] = getBandFreqRange(1);
        const auto [f2_0, f2_1] = getBandFreqRange(2);
        if (freq < f0_1) return 0;
        if (freq < f1_1) return 1;
        if (freq < f2_1) return 2;
        return 3;
    }

    [[nodiscard]] std::pair<float, float> getBandFreqRange(size_t b) const noexcept {
        const float c0 = std::clamp(crossovers[0], 20.0f, 17500.0f);
        const float c1 = std::clamp(crossovers[1], c0 * 1.05f + 5.0f, 18500.0f);
        const float c2 = std::clamp(crossovers[2], c1 * 1.05f + 5.0f, 20000.0f);

        if (b == 0) return { 20.0f, c0 };
        if (b == 1) return { c0, c1 };
        if (b == 2) return { c1, c2 };
        return { c2, 20000.0f };
    }

    [[nodiscard]] juce::Point<float> getBandCenterPoint(size_t b, const juce::Rectangle<float>& bounds) const noexcept {
        const auto [f0, f1] = getBandFreqRange(b);
        const float geometricCenterFreq = std::sqrt(f0 * f1);
        const float x = freqToX(geometricCenterFreq, bounds);
        const float y = gainToY(bands[b].makeupGainDb, bounds);
        return { x, y };
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        // Vertical frequency grid
        static constexpr float Frequencies[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 20000.0f };
        static constexpr const char* FreqLabels[] = { "50", "100", "250", "500", "1k", "2.5k", "5k", "10k", "20k" };

        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));

        for (size_t i = 0; i < std::size(Frequencies); ++i) {
            const float x = freqToX(Frequencies[i], bounds);
            g.setColour(juce::Colour(0xff1d222b));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            g.setColour(juce::Colour(0xff4a5468));
            g.drawText(FreqLabels[i], static_cast<int>(x - 20), static_cast<int>(bounds.getBottom() - 16), 40, 14, juce::Justification::centred);
        }

        // Horizontal gain grid
        static constexpr float Gains[] = { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f };
        for (float gDb : Gains) {
            const float y = gainToY(gDb, bounds);
            if (gDb == 0.0f) {
                g.setColour(juce::Colour(0xff333d4d));
            } else {
                g.setColour(juce::Colour(0xff1a1e27));
            }
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());

            g.setColour(juce::Colour(0xff4a5468));
            const juce::String label = (gDb > 0 ? "+" : "") + juce::String(static_cast<int>(gDb));
            g.drawText(label, static_cast<int>(bounds.getRight() - 32), static_cast<int>(y - 7), 28, 14, juce::Justification::right);
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
            const float y = bounds.getBottom() - norm * (bounds.getHeight() * 0.85f);

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

        juce::ColourGradient grad(juce::Colour(0x1800f0ff), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour(0x0200f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(spectrumPath);

        g.setColour(juce::Colour(0x3500f0ff));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }

    void drawBandRegions(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        bool anySolo = false;
        for (size_t s = 0; s < NumBands; ++s) {
            if (bands[s].solo) { anySolo = true; break; }
        }

        for (size_t b = 0; b < NumBands; ++b) {
            const auto [f0, f1] = getBandFreqRange(b);
            const float x0 = freqToX(f0, bounds);
            const float x1 = freqToX(f1, bounds);
            const auto regionRect = juce::Rectangle<float>(x0, bounds.getY(), std::max(0.0f, x1 - x0), bounds.getHeight());

            const bool isSilenced = bands[b].mute || (anySolo && !bands[b].solo);
            const bool isSelected = (selectedBand == b);
            const auto baseColour = bands[b].colour;

            // 1. Region background tint
            const uint8_t alpha = isSelected ? 0x22 : (isSilenced ? 0x05 : 0x0c);
            g.setColour(baseColour.withAlpha(static_cast<float>(alpha) / 255.0f));
            g.fillRect(regionRect);

            if (isSelected) {
                // Subtle highlight top line
                g.setColour(baseColour.withAlpha(0.6f));
                g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1), x0, x1);
            }

            // 2. Band Header Pill (Name & status)
            const float pillWidth = std::min(70.0f, (x1 - x0) - 8.0f);
            if (pillWidth > 30.0f) {
                const auto pillRect = juce::Rectangle<float>(x0 + 4.0f, bounds.getY() + 6.0f, pillWidth, 18.0f);
                g.setColour(isSelected ? baseColour.withAlpha(0.3f) : juce::Colour(0x22ffffff));
                g.fillRoundedRectangle(pillRect, 4.0f);

                g.setColour(isSelected ? juce::Colours::white : juce::Colour(0xffabb2bf));
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(bands[b].name, pillRect, juce::Justification::centred);

                // Solo/Mute/Bypass indicator
                if (bands[b].solo) {
                    const auto soloRect = juce::Rectangle<float>(pillRect.getRight() + 4.0f, pillRect.getY(), 18.0f, 18.0f);
                    g.setColour(juce::Colour(0xffffb300));
                    g.fillRoundedRectangle(soloRect, 3.0f);
                    g.setColour(juce::Colours::black);
                    g.drawText("S", soloRect, juce::Justification::centred);
                } else if (bands[b].mute) {
                    const auto muteRect = juce::Rectangle<float>(pillRect.getRight() + 4.0f, pillRect.getY(), 18.0f, 18.0f);
                    g.setColour(juce::Colour(0xffff5252));
                    g.fillRoundedRectangle(muteRect, 3.0f);
                    g.setColour(juce::Colours::white);
                    g.drawText("M", muteRect, juce::Justification::centred);
                } else if (bands[b].bypass) {
                    const auto bypRect = juce::Rectangle<float>(pillRect.getRight() + 4.0f, pillRect.getY(), 26.0f, 18.0f);
                    g.setColour(juce::Colour(0xff5c6370));
                    g.fillRoundedRectangle(bypRect, 3.0f);
                    g.setColour(juce::Colours::white);
                    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
                    g.drawText("BYP", bypRect, juce::Justification::centred);
                }
            }

            // 3. Baseline makeup gain line
            const float yGain = gainToY(bands[b].makeupGainDb, bounds);
            g.setColour(baseColour.withAlpha(isSelected ? 0.5f : (isSilenced ? 0.15f : 0.25f)));
            static constexpr float dashedPattern[2] = { 4.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(x0, yGain, x1, yGain), dashedPattern, 2, 1.0f);

            // 4. Dynamic Range Bracket
            const float yRange = gainToY(bands[b].makeupGainDb + bands[b].rangeDb, bounds);
            const float bracketX = x0 + 6.0f;
            g.setColour(baseColour.withAlpha(isSilenced ? 0.15f : 0.35f));
            g.drawVerticalLine(static_cast<int>(bracketX), std::min(yGain, yRange), std::max(yGain, yRange));
            g.drawHorizontalLine(static_cast<int>(yRange), bracketX - 3.0f, bracketX + 3.0f);

            // 5. Real-Time Dynamic Gain Reduction / Expansion Ribbon
            const float grDb = bands[b].currentGainChangeDb;
            if (std::abs(grDb) > 0.1f && !isSilenced && !bands[b].bypass) {
                const float yDyn = gainToY(bands[b].makeupGainDb + grDb, bounds);
                const float topY = std::min(yGain, yDyn);
                const float ribbonHeight = std::abs(yDyn - yGain);

                const auto ribbonRect = juce::Rectangle<float>(x0 + 2.0f, topY, std::max(0.0f, (x1 - x0) - 4.0f), ribbonHeight);

                // Compression is red/coral, expansion is neon green/teal
                const juce::Colour ribbonColor = (grDb < 0.0f)
                    ? juce::Colour(0xffff5252)
                    : juce::Colour(0xff00e676);

                juce::ColourGradient ribbonGrad(ribbonColor.withAlpha(0.45f), ribbonRect.getCentreX(), topY,
                                                ribbonColor.withAlpha(0.12f), ribbonRect.getCentreX(), topY + ribbonHeight, false);
                g.setGradientFill(ribbonGrad);
                g.fillRoundedRectangle(ribbonRect, 2.0f);

                // Glowing edge line at active dynamic excursion point
                g.setColour(ribbonColor.withAlpha(0.85f));
                g.drawHorizontalLine(static_cast<int>(yDyn), x0 + 2.0f, x1 - 2.0f);

                // Dynamic gain readout badge
                if ((x1 - x0) > 60.0f) {
                    const juce::String grText = (grDb > 0 ? "+" : "") + juce::String(grDb, 1) + " dB";
                    g.setColour(ribbonColor);
                    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                    g.drawText(grText, static_cast<int>(x1 - 54.0f), static_cast<int>(yDyn - 14.0f), 50, 12, juce::Justification::right);
                }
            }
        }
    }

    void drawCompositeCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path curvePath;
        const int numPoints = static_cast<int>(bounds.getWidth());
        if (numPoints <= 0) return;

        bool anySolo = false;
        for (size_t s = 0; s < NumBands; ++s) {
            if (bands[s].solo) { anySolo = true; break; }
        }

        bool firstPoint = true;
        for (int i = 0; i < numPoints; i += 2) {
            const float x = bounds.getX() + static_cast<float>(i);
            const float freq = screenToFreq(x, bounds);

            // Compute composite frequency response with smooth crossover blending
            float totalGainDb = 0.0f;
            float totalWeight = 0.0f;

            for (size_t b = 0; b < NumBands; ++b) {
                const auto [f0, f1] = getBandFreqRange(b);

                float bandEffectiveGain = 0.0f;
                if (bands[b].mute || (anySolo && !bands[b].solo)) {
                    bandEffectiveGain = -48.0f;
                } else if (bands[b].bypass) {
                    bandEffectiveGain = 0.0f;
                } else {
                    bandEffectiveGain = bands[b].makeupGainDb + bands[b].currentGainChangeDb;
                }

                // 4th order Linkwitz-Riley crossover attenuation curves
                float weight = 1.0f;
                if (b > 0) {
                    // High-pass response from lower crossover
                    const float ratioLo = freq / f0;
                    const float ratioLoSq = ratioLo * ratioLo;
                    weight *= (ratioLoSq * ratioLoSq) / (1.0f + ratioLoSq * ratioLoSq);
                }
                if (b < NumBands - 1) {
                    // Low-pass response from upper crossover
                    const float ratioHi = freq / f1;
                    const float ratioHiSq = ratioHi * ratioHi;
                    weight *= 1.0f / (1.0f + ratioHiSq * ratioHiSq);
                }

                totalGainDb += bandEffectiveGain * weight;
                totalWeight += weight;
            }

            if (totalWeight > 1e-4f) {
                totalGainDb /= totalWeight;
            }

            const float y = gainToY(totalGainDb, bounds);

            if (firstPoint) {
                curvePath.startNewSubPath(x, y);
                firstPoint = false;
            } else {
                curvePath.lineTo(x, y);
            }
        }

        // Iconic FabFilter Pro-MB yellow dynamic curve
        g.setColour(juce::Colour(0xffffd600));
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawBandHandles(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        bool anySolo = false;
        for (size_t s = 0; s < NumBands; ++s) {
            if (bands[s].solo) { anySolo = true; break; }
        }

        for (size_t b = 0; b < NumBands; ++b) {
            const auto pt = getBandCenterPoint(b, bounds);
            const bool isSelected = (selectedBand == b);
            const bool isHovered = (hoveredBandNode == static_cast<int>(b));
            const bool isSilenced = bands[b].mute || (anySolo && !bands[b].solo);
            const auto baseColour = isSilenced ? bands[b].colour.withAlpha(0.35f) : bands[b].colour;

            const float radius = (isSelected || isHovered) ? 8.0f : 6.0f;

            // Outer glow ring
            if (isSelected) {
                g.setColour(baseColour.withAlpha(0.4f));
                g.fillEllipse(pt.x - radius - 4.0f, pt.y - radius - 4.0f, (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
            }

            // Fill
            g.setColour(baseColour);
            g.fillEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);

            // Inner center dot
            g.setColour(isSilenced ? juce::Colours::grey : juce::Colours::white);
            g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);

            // Border
            g.setColour(juce::Colours::white.withAlpha(isSilenced ? 0.4f : 0.9f));
            g.drawEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }
    }

    void drawCrossoverDividers(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        for (size_t i = 0; i < NumCrossovers; ++i) {
            const float x = freqToX(crossovers[i], bounds);
            const bool isHovered = (hoveredCrossover == static_cast<int>(i));
            const bool isDragging = (draggingCrossover == static_cast<int>(i));

            // Vertical divider line
            g.setColour(isHovered || isDragging ? juce::Colours::white : juce::Colour(0x66abb2bf));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            // Handle badge at top
            const float handleWidth = 44.0f;
            const float handleHeight = 20.0f;
            const auto handleRect = juce::Rectangle<float>(x - handleWidth * 0.5f, bounds.getY() + 2.0f, handleWidth, handleHeight);

            g.setColour(isHovered || isDragging ? juce::Colour(0xff2c313c) : juce::Colour(0xee1e222b));
            g.fillRoundedRectangle(handleRect, 4.0f);

            g.setColour(isHovered || isDragging ? juce::Colour(0xff00e5ff) : juce::Colour(0xff5c6370));
            g.drawRoundedRectangle(handleRect, 4.0f, 1.0f);

            // Frequency text
            juce::String freqStr;
            if (crossovers[i] >= 1000.0f) {
                freqStr = juce::String(crossovers[i] / 1000.0f, 1) + "k";
            } else {
                freqStr = juce::String(static_cast<int>(std::round(crossovers[i])));
            }

            g.setColour(isHovered || isDragging ? juce::Colours::white : juce::Colour(0xffabb2bf));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(freqStr, handleRect, juce::Justification::centred);
        }
    }
};

} // namespace openx::ui
