#pragma once

#include <cmath>
#include <numbers>
#include <concepts>
#include <algorithm>

namespace openx::dsp {

template <std::floating_point T>
class DcBlocker {
public:
    void prepare(T sampleRate) noexcept {
        sr = std::max(sampleRate, T{1000.0});
        setCutoff(T{10.0});
        reset();
    }

    void setCutoff(T cutoffHz) noexcept {
        const T fc = std::clamp(cutoffHz, T{2.0}, T{40.0});
        const T omega = std::numbers::pi_v<T> * fc / sr;
        const T k = std::tan(omega);
        const T k2 = k * k;
        constexpr T q = T{0.7071067811865475}; // Butterworth Q
        const T a0 = T{1.0} + k / q + k2;

        b0 = T{1.0} / a0;
        b1 = T{-2.0} / a0;
        b2 = T{1.0} / a0;
        a1 = T{2.0} * (k2 - T{1.0}) / a0;
        a2 = (T{1.0} - k / q + k2) / a0;
    }

    void reset() noexcept {
        s1 = T{0};
        s2 = T{0};
    }

    [[nodiscard]] inline T processSample(T x) noexcept {
        const T y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

private:
    T sr{44100};
    T b0{1}, b1{-2}, b2{1}, a1{0}, a2{0};
    T s1{0}, s2{0};
};

} // namespace openx::dsp
