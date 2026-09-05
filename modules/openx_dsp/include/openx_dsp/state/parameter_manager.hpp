#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <span>
#include <concepts>
#include <string_view>
#include <type_traits>

namespace openx::state {

struct ParameterDescriptor {
    size_t index;
    std::string_view id;
    std::string_view name;
    float minValue;
    float maxValue;
    float defaultValue;
    float skewFactor{1.0f};
    std::string_view unit{""};
};

template <typename EnumT, size_t ParamCount>
    requires std::is_enum_v<EnumT>
class ParameterManager {
public:
    static_assert(ParamCount > 0, "Parameter count must be positive");

    using LayoutDescriptors = std::array<ParameterDescriptor, ParamCount>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(
        const LayoutDescriptors& descriptors) 
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (const auto& desc : descriptors) {
            juce::NormalisableRange<float> range(desc.minValue, desc.maxValue);
            range.skew = desc.skewFactor;

            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(juce::String(desc.id.data(), desc.id.size()), 1),
                juce::String(desc.name.data(), desc.name.size()),
                range,
                desc.defaultValue,
                juce::AudioParameterFloatAttributes().withLabel(juce::String(desc.unit.data(), desc.unit.size()))
            ));
        }
        return layout;
    }

    void initialize(juce::AudioProcessorValueTreeState& apvts, const LayoutDescriptors& descriptors) noexcept {
        for (const auto& desc : descriptors) {
            auto rawPtr = apvts.getRawParameterValue(juce::String(desc.id.data(), desc.id.size()));
            jassert(rawPtr != nullptr);
            cachedPointers[desc.index] = rawPtr;
        }
    }

    [[nodiscard]] inline float get(EnumT paramId) const noexcept {
        const auto idx = static_cast<size_t>(paramId);
        jassert(idx < ParamCount);
        return cachedPointers[idx]->load(std::memory_order_relaxed);
    }

    [[nodiscard]] inline std::atomic<float>* getRaw(EnumT paramId) const noexcept {
        const auto idx = static_cast<size_t>(paramId);
        jassert(idx < ParamCount);
        return cachedPointers[idx];
    }

private:
    alignas(64) std::array<std::atomic<float>*, ParamCount> cachedPointers{};
};

} // namespace openx::state
