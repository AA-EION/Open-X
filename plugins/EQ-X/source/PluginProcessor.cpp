#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::eq {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& chBiquads : dynBiquads) {
        for (auto& biquad : chBiquads) {
            biquad.prepare(static_cast<float>(sampleRate));
        }
    }
}

void PluginProcessor::releaseResources() {
    for (auto& chBiquads : dynBiquads) {
        for (auto& biquad : chBiquads) {
            biquad.reset();
        }
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float inGainLinear  = juce::Decibels::decibelsToGain(params.get(ParamId::InputGain));
    const float outGainLinear = juce::Decibels::decibelsToGain(params.get(ParamId::OutputGain));

    // Check if any band has solo enabled
    bool anySolo = false;
    for (size_t b = 0; b < NumBands; ++b) {
        const bool solo = params.get(getBandSoloId(b)) > 0.5f;
        const bool bypass = params.get(getBandBypassId(b)) > 0.5f;
        if (solo && !bypass) {
            anySolo = true;
            break;
        }
    }

    // Configure all 8 dynamic biquad engines across both channels
    for (size_t b = 0; b < NumBands; ++b) {
        openx::dsp::DynamicBiquadEngine<float>::Parameters p;
        p.frequency        = params.get(getBandFreqId(b));
        p.staticGainDb     = params.get(getBandGainId(b));
        p.q                = params.get(getBandQId(b));
        p.dynamicGainMaxDb = params.get(getBandDynGainId(b));
        p.thresholdDb      = params.get(getBandThresholdId(b));
        p.ratio            = 2.0f;
        p.kneeDb           = 3.0f;
        p.attackMs         = 10.0f;
        p.releaseMs        = 100.0f;
        p.downward         = (p.dynamicGainMaxDb <= 0.0f);
        p.bypassed         = (params.get(getBandBypassId(b)) > 0.5f);

        const int typeInt = std::clamp(static_cast<int>(std::round(params.get(getBandTypeId(b)))), 0, 5);
        p.filterType       = static_cast<openx::dsp::DynamicBiquadEngine<float>::FilterType>(typeInt);

        // LowCut, HighCut, and Notch filters have fixed 0 dB passband gain
        if (p.filterType == openx::dsp::DynamicBiquadEngine<float>::FilterType::LowCut ||
            p.filterType == openx::dsp::DynamicBiquadEngine<float>::FilterType::HighCut ||
            p.filterType == openx::dsp::DynamicBiquadEngine<float>::FilterType::Notch) {
            p.staticGainDb = 0.0f;
        }

        for (size_t ch = 0; ch < 2; ++ch) {
            dynBiquads[ch][b].setParameters(p);
        }
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& chBiquads = dynBiquads[static_cast<size_t>(ch)];

        if (anySolo) {
            // Solo audition mode: sum isolated frequency bands of active soloed bands
            for (int i = 0; i < numSamples; ++i) {
                const float in = channelData[i] * inGainLinear;
                float soloSum = 0.0f;
                for (size_t b = 0; b < NumBands; ++b) {
                    const bool solo = params.get(getBandSoloId(b)) > 0.5f;
                    const bool bypass = params.get(getBandBypassId(b)) > 0.5f;
                    if (solo && !bypass) {
                        soloSum += chBiquads[b].processSoloSample(in);
                    }
                }
                const float finalSample = soloSum * outGainLinear;
                channelData[i] = finalSample;

                if (ch == 0) {
                    spectrumAnalyzer.pushSample(finalSample);
                }
            }
        } else {
            // Normal mode: cascade all 8 dynamic biquads in series
            for (int i = 0; i < numSamples; ++i) {
                float sample = channelData[i] * inGainLinear;
                for (size_t b = 0; b < NumBands; ++b) {
                    sample = chBiquads[b].processSample(sample);
                }
                const float finalSample = sample * outGainLinear;
                channelData[i] = finalSample;

                if (ch == 0) {
                    spectrumAnalyzer.pushSample(finalSample);
                }
            }
        }
    }

    // Cache dynamic gain modulation offsets for UI meters (from channel 0)
    for (size_t b = 0; b < NumBands; ++b) {
        dynamicGainOffsets[b].store(dynBiquads[0][b].getDynamicDeltaDb(), std::memory_order_relaxed);
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace openx::eq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::eq::PluginProcessor();
}
