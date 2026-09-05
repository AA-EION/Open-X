#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace openx::mb {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& proc : processors) {
        proc.prepare(static_cast<float>(sampleRate));
    }
}

void PluginProcessor::releaseResources() {
    for (auto& proc : processors) {
        proc.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float inGainDb  = params.get(ParamId::InputGain);
    const float outGainDb = params.get(ParamId::OutputGain);
    const float inGainLinear  = std::pow(10.0f, inGainDb / 20.0f);
    const float outGainLinear = std::pow(10.0f, outGainDb / 20.0f);

    openx::dsp::MultibandProcessor<float, 4>::Parameters p;
    p.crossoverFrequenciesHz[0] = params.get(ParamId::Crossover1);
    p.crossoverFrequenciesHz[1] = params.get(ParamId::Crossover2);
    p.crossoverFrequenciesHz[2] = params.get(ParamId::Crossover3);

    auto fillBand = [this, &p](size_t b, ParamId mode, ParamId thresh, ParamId range,
                               ParamId ratio, ParamId attack, ParamId release,
                               ParamId knee, ParamId gain, ParamId solo,
                               ParamId mute, ParamId bypass) {
        auto& bp = p.bands[b];
        bp.mode = (params.get(mode) > 0.5f)
            ? openx::dsp::MultibandDynamicsEngine<float>::DynamicsMode::Expand
            : openx::dsp::MultibandDynamicsEngine<float>::DynamicsMode::Compress;
        bp.thresholdDb  = params.get(thresh);
        bp.rangeDb      = params.get(range);
        bp.ratio        = params.get(ratio);
        bp.attackMs     = params.get(attack);
        bp.releaseMs    = params.get(release);
        bp.kneeDb       = params.get(knee);
        bp.makeupGainDb = params.get(gain);
        bp.solo         = (params.get(solo) > 0.5f);
        bp.mute         = (params.get(mute) > 0.5f);
        bp.bypass       = (params.get(bypass) > 0.5f);
    };

    fillBand(0, ParamId::B0_Mode, ParamId::B0_Thresh, ParamId::B0_Range, ParamId::B0_Ratio,
             ParamId::B0_Attack, ParamId::B0_Release, ParamId::B0_Knee, ParamId::B0_Gain,
             ParamId::B0_Solo, ParamId::B0_Mute, ParamId::B0_Bypass);

    fillBand(1, ParamId::B1_Mode, ParamId::B1_Thresh, ParamId::B1_Range, ParamId::B1_Ratio,
             ParamId::B1_Attack, ParamId::B1_Release, ParamId::B1_Knee, ParamId::B1_Gain,
             ParamId::B1_Solo, ParamId::B1_Mute, ParamId::B1_Bypass);

    fillBand(2, ParamId::B2_Mode, ParamId::B2_Thresh, ParamId::B2_Range, ParamId::B2_Ratio,
             ParamId::B2_Attack, ParamId::B2_Release, ParamId::B2_Knee, ParamId::B2_Gain,
             ParamId::B2_Solo, ParamId::B2_Mute, ParamId::B2_Bypass);

    fillBand(3, ParamId::B3_Mode, ParamId::B3_Thresh, ParamId::B3_Range, ParamId::B3_Ratio,
             ParamId::B3_Attack, ParamId::B3_Release, ParamId::B3_Knee, ParamId::B3_Gain,
             ParamId::B3_Solo, ParamId::B3_Mute, ParamId::B3_Bypass);

    for (auto& proc : processors) {
        proc.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& proc = processors[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float in = channelData[i] * inGainLinear;
            const float out = proc.processSample(in) * outGainLinear;
            channelData[i] = out;
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    // Push stereo average into spectrum analyzer to capture all active audio
    for (int i = 0; i < numSamples; ++i) {
        float sum = 0.0f;
        for (int ch = 0; ch < channelsToProcess; ++ch) {
            sum += buffer.getSample(ch, i);
        }
        sum /= static_cast<float>(std::max(channelsToProcess, 1));
        spectrumAnalyzer.pushSample(sum);
    }
}

float PluginProcessor::getBandGainReductionDb(size_t band) const noexcept {
    if (band < 4) {
        if (getTotalNumInputChannels() == 1) {
            return processors[0].getBandGainChangeDb(band);
        }
        const float g0 = processors[0].getBandGainChangeDb(band);
        const float g1 = processors[1].getBandGainChangeDb(band);
        if (g0 < 0.0f || g1 < 0.0f) {
            return std::min(g0, g1);
        }
        return std::max(g0, g1);
    }
    return 0.0f;
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
        // Migration mapping for legacy parameter identifiers
        for (auto* child : xmlState->getChildIterator()) {
            if (child->hasTagName("PARAM")) {
                const auto id = child->getStringAttribute("id");
                if (id == "low_thresh") child->setAttribute("id", "b0_thresh");
                else if (id == "mid_thresh") child->setAttribute("id", "b1_thresh");
                else if (id == "hi_thresh")  child->setAttribute("id", "b3_thresh");
                else if (id == "low_gain")   child->setAttribute("id", "b0_gain");
                else if (id == "mid_gain")   child->setAttribute("id", "b1_gain");
                else if (id == "hi_gain")    child->setAttribute("id", "b3_gain");
                else if (id == "low_mid_xo") child->setAttribute("id", "xo1");
                else if (id == "mid_hi_xo")  child->setAttribute("id", "xo2");
            }
        }
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace openx::mb

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::mb::PluginProcessor();
}
