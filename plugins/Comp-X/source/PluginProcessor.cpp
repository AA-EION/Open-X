#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::comp {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    compressor.prepare(static_cast<float>(sampleRate));

    const float lookaheadMs = params.get(ParamId::Lookahead);
    const int latency = static_cast<int>(std::round(lookaheadMs * 0.001 * sampleRate));
    setLatencySamples(latency);
}

void PluginProcessor::releaseResources() {
    compressor.reset();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // 1. Gather APVTS parameters
    openx::dsp::CompressorEngine<float>::Parameters p;
    p.thresholdDb     = params.get(ParamId::Threshold);
    p.ratio           = params.get(ParamId::Ratio);
    p.kneeDb          = params.get(ParamId::Knee);
    p.attackMs        = params.get(ParamId::Attack);
    p.releaseMs       = params.get(ParamId::Release);
    p.autoRelease     = params.get(ParamId::AutoRelease) > 0.5f;
    p.holdMs          = params.get(ParamId::Hold);
    p.style           = static_cast<openx::dsp::CompressionStyle>(
                            std::clamp(static_cast<int>(params.get(ParamId::Style) + 0.5f), 0, 7));
    p.lookaheadMs     = params.get(ParamId::Lookahead);
    p.scHpfHz         = params.get(ParamId::ScHpf);
    p.scLpfHz         = params.get(ParamId::ScLpf);
    p.scAudition      = params.get(ParamId::ScAudition) > 0.5f;
    p.makeupGainDb    = params.get(ParamId::Makeup);
    p.autoGain        = params.get(ParamId::AutoGain) > 0.5f;
    p.mix             = params.get(ParamId::Mix) * 0.01f;
    p.transientPunch  = params.get(ParamId::Punch);

    compressor.setParameters(p);

    // 2. Dynamically report host latency for lookahead delay compensation
    const int requiredLatency = static_cast<int>(compressor.getLookaheadSamples());
    if (getLatencySamples() != requiredLatency) {
        setLatencySamples(requiredLatency);
    }

    const int numSamples = buffer.getNumSamples();
    float maxInLinear = 0.0f;
    float maxOutLinear = 0.0f;

    // 3. Audio processing
    if (totalNumInputChannels >= 2) {
        auto* ch0 = buffer.getWritePointer(0);
        auto* ch1 = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i) {
            const float inL = ch0[i];
            const float inR = ch1[i];
            float outL = 0.0f;
            float outR = 0.0f;

            compressor.processStereo(inL, inR, outL, outR);

            ch0[i] = outL;
            ch1[i] = outR;

            const float absIn = std::max(std::abs(inL), std::abs(inR));
            const float absOut = std::max(std::abs(outL), std::abs(outR));
            if (absIn > maxInLinear) maxInLinear = absIn;
            if (absOut > maxOutLinear) maxOutLinear = absOut;
        }
    } else if (totalNumInputChannels == 1) {
        auto* ch0 = buffer.getWritePointer(0);

        for (int i = 0; i < numSamples; ++i) {
            const float in = ch0[i];
            const float out = compressor.processSample(in);
            ch0[i] = out;

            const float absIn = std::abs(in);
            const float absOut = std::abs(out);
            if (absIn > maxInLinear) maxInLinear = absIn;
            if (absOut > maxOutLinear) maxOutLinear = absOut;
        }

        if (totalNumOutputChannels > 1) {
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
        }
    }

    // 4. Update visualizer history buffer
    const float inDb = juce::Decibels::gainToDecibels(maxInLinear, -100.0f);
    const float outDb = juce::Decibels::gainToDecibels(maxOutLinear, -100.0f);
    const float grDb = compressor.getGainReductionDb();
    history.push(inDb, outDb, grDb);
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

} // namespace openx::comp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::comp::PluginProcessor();
}
