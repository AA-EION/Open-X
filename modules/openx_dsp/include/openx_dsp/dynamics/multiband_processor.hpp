#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include "../crossover/linkwitz_riley.hpp"
#include "compressor_engine.hpp"

namespace openx::dsp {

template <std::floating_point T>
class MultibandProcessor3Band {
public:
    struct BandParameters {
        T thresholdDb{-20};
        T ratio{3};
        T attackMs{20};
        T releaseMs{100};
        T gainDb{0};
    };

    struct Parameters {
        T lowMidCrossoverHz{250.0};
        T midHighCrossoverHz{3500.0};
        std::array<BandParameters, 3> bands;
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        splitter.prepare(sr);
        for (auto& comp : bandCompressors) comp.prepare(sr);
        reset();
    }

    void reset() noexcept {
        splitter.reset();
        for (auto& comp : bandCompressors) comp.reset();
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        const std::array<T, 2> cutoffs{ params.lowMidCrossoverHz, params.midHighCrossoverHz };
        splitter.setFrequencies(cutoffs);

        for (size_t b = 0; b < 3; ++b) {
            typename CompressorEngine<T>::Parameters cp;
            cp.thresholdDb = params.bands[b].thresholdDb;
            cp.ratio = params.bands[b].ratio;
            cp.attackMs = params.bands[b].attackMs;
            cp.releaseMs = params.bands[b].releaseMs;
            cp.makeupGainDb = params.bands[b].gainDb;
            cp.kneeDb = T{4};
            cp.transientPunch = T{0.2};
            bandCompressors[b].setParameters(cp);
        }
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Split input into 3 phase-aligned bands using LR4 + Allpass compensation
        std::array<T, 3> splitBands{};
        splitter.process(input, splitBands);

        // 2. Process each band through decoupled dynamics
        T output = 0;
        for (size_t b = 0; b < 3; ++b) {
            const T processed = bandCompressors[b].processSample(splitBands[b]);
            output += processed;
        }

        return output;
    }

private:
    T sr{44100};
    Parameters params{};
    PhaseAlignedMultibandSplitter<T, 3> splitter;
    std::array<CompressorEngine<T>, 3> bandCompressors;
};

} // namespace openx::dsp
