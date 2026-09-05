#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::verb {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    reverb.prepare(static_cast<float>(sampleRate));
}

void PluginProcessor::releaseResources() {
    reverb.reset();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    openx::dsp::FdnReverb<float, 16>::Parameters p;
    p.decayTimeSec = params.get(ParamId::DecayTime);
    p.dampingHz = params.get(ParamId::Damping);
    p.chaosModulation = params.get(ParamId::ChaosMod);
    p.dryWet = params.get(ParamId::Mix);
    reverb.setParameters(p);

    const int numSamples = buffer.getNumSamples();
    auto* leftChannel = (totalNumOutputChannels > 0) ? buffer.getWritePointer(0) : nullptr;
    auto* rightChannel = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;
    const auto* leftIn = (totalNumInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const auto* rightIn = (totalNumInputChannels > 1) ? buffer.getReadPointer(1) : leftIn;

    if (leftChannel == nullptr) return;

    for (int i = 0; i < numSamples; ++i) {
        const float inL = (leftIn != nullptr) ? leftIn[i] : 0.0f;
        const float inR = (rightIn != nullptr) ? rightIn[i] : inL;
        const auto [outL, outR] = reverb.processSample(inL, inR);
        if (rightChannel != nullptr) {
            leftChannel[i] = outL;
            rightChannel[i] = outR;
        } else {
            leftChannel[i] = (outL + outR) * 0.70710678f;
        }

        spectrumAnalyzer.pushSample((outL + outR) * 0.5f);
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

} // namespace openx::verb

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::verb::PluginProcessor();
}
