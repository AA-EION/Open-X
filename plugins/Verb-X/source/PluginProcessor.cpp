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

    // 1. Host Tempo Sync Calculation for Pre-Delay
    float predelayMs = params.get(ParamId::PreDelay);
    const bool isSync = params.get(ParamId::PreDelaySync) > 0.5f;

    if (isSync) {
        double bpm = 120.0;
        auto* playHead = getPlayHead();
        if (playHead != nullptr) {
            auto posInfo = playHead->getPosition();
            if (posInfo.hasValue() && posInfo->getBpm().hasValue()) {
                bpm = *posInfo->getBpm();
            }
        }
        bpm = std::clamp(bpm, 20.0, 400.0);
        const int noteIdx = std::clamp(static_cast<int>(std::round(params.get(ParamId::PreDelayNote))), 0, 7);

        // Note beat multipliers relative to quarter note (1 beat):
        // 0: 1/32, 1: 1/16, 2: 1/8T, 3: 1/16D, 4: 1/8, 5: 1/4T, 6: 1/8D, 7: 1/4
        static constexpr double NoteBeats[] = {
            0.125,
            0.25,
            1.0 / 3.0,
            0.375,
            0.5,
            2.0 / 3.0,
            0.75,
            1.0
        };

        const double beats = NoteBeats[noteIdx];
        const double secondsPerBeat = 60.0 / bpm;
        predelayMs = static_cast<float>(beats * secondsPerBeat * 1000.0);
    }

    // 2. Configure 16-Channel Householder FDN Reverb DSP Parameters
    openx::dsp::FdnReverb<float, 16>::Parameters p;
    p.decayTimeSec = params.get(ParamId::DecayTime);
    p.space = params.get(ParamId::Space);
    p.predelayMs = predelayMs;
    p.distance = params.get(ParamId::Distance);
    p.diffusion = params.get(ParamId::Diffusion);
    p.stereoWidth = params.get(ParamId::Width);
    p.dampingHz = params.get(ParamId::Damping);
    p.lowCutHz = params.get(ParamId::LowCut);
    p.ducking = params.get(ParamId::Ducking);
    p.decayRateLow = params.get(ParamId::DecayLow);
    p.decayRateLowFreq = params.get(ParamId::DecayLowFreq);
    p.decayRateMid = params.get(ParamId::DecayMid);
    p.decayRateMidFreq = params.get(ParamId::DecayMidFreq);
    p.decayRateMidQ = params.get(ParamId::DecayMidQ);
    p.decayRateHigh = params.get(ParamId::DecayHigh);
    p.decayRateHighFreq = params.get(ParamId::DecayHighFreq);
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
    if (xml != nullptr) {
        copyXmlToBinary(*xml, destData);
    }
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
