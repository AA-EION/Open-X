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
    for (auto& biquad : dynBiquads) {
        biquad.prepare(static_cast<float>(sampleRate));
    }
}

void PluginProcessor::releaseResources() {
    for (auto& biquad : dynBiquads) {
        biquad.reset();
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

    openx::dsp::DynamicBiquadEngine<float>::Parameters p;
    p.frequency        = params.get(ParamId::Band1Freq);
    p.staticGainDb     = params.get(ParamId::Band1Gain);
    p.q                = params.get(ParamId::Band1Q);
    p.dynamicGainMaxDb = params.get(ParamId::Band1DynGain);
    p.thresholdDb      = params.get(ParamId::Band1Threshold);
    p.ratio            = 2.0f;
    p.kneeDb           = 3.0f;
    p.attackMs         = 10.0f;
    p.releaseMs        = 100.0f;
    p.downward         = (p.dynamicGainMaxDb <= 0.0f);

    for (auto& biquad : dynBiquads) {
        biquad.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& engine = dynBiquads[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float in = channelData[i] * inGainLinear;
            const float out = engine.processSample(in);
            const float finalSample = out * outGainLinear;
            channelData[i] = finalSample;

            if (ch == 0) {
                spectrumAnalyzer.pushSample(finalSample);
            }
        }
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
