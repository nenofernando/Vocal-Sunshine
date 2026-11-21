#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

NFVocalSuiteAudioProcessor::NFVocalSuiteAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NF_VOCAL_SUITE_STATE", createParameters())
{
    for (int s = 0; s < nfvs::kStages; ++s)
    {
        amtP[s] = apvts.getRawParameterValue(stageId(s, "amt")); modeP[s] = apvts.getRawParameterValue(stageId(s, "mode"));
        onP[s] = apvts.getRawParameterValue(stageId(s, "on"));   aP[s] = apvts.getRawParameterValue(stageId(s, "a"));
        bP[s] = apvts.getRawParameterValue(stageId(s, "b"));
    }
    outP = apvts.getRawParameterValue("output"); mixP = apvts.getRawParameterValue("mix");
    lowP = apvts.getRawParameterValue("lowcut"); highP = apvts.getRawParameterValue("highcut"); bypassP = apvts.getRawParameterValue("bypass");
}

void NFVocalSuiteAudioProcessor::prepareToPlay(double sr, int maxBlock)
{
    chain.prepare(sr);
    bypassMix.reset(sr, 0.015);
    bypassMix.setCurrentAndTargetValue(bypassP->load() > 0.5f ? 1.0f : 0.0f);
    for (auto& v : scratchIn) v.assign((size_t) std::max(1, maxBlock) + 64, 0.0f);
}

bool NFVocalSuiteAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

void NFVocalSuiteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    const int numCh = std::min(2, buffer.getNumChannels()), n = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0) return;

    nfvs::Params prm;
    for (int s = 0; s < nfvs::kStages; ++s)
    {
        prm.amt[s] = amtP[s]->load(); prm.mode[s] = juce::roundToInt(modeP[s]->load());
        prm.on[s] = onP[s]->load() > 0.5f; prm.a[s] = aP[s]->load() > 0.5f; prm.b[s] = bP[s]->load() > 0.5f;
    }
    prm.outputDb = outP->load(); prm.mix = mixP->load(); prm.lowcutHz = lowP->load(); prm.highcutHz = highP->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) prm.bpm = *bpm;
    bypassMix.setTargetValue(bypassP->load() > 0.5f ? 1.0f : 0.0f);

    if ((int) scratchIn[0].size() < n) for (auto& v : scratchIn) v.resize((size_t) n);
    for (int ch = 0; ch < numCh; ++ch) std::copy(buffer.getReadPointer(ch), buffer.getReadPointer(ch) + n, scratchIn[ch].data());

    float* l = buffer.getWritePointer(0);
    if (numCh > 1) chain.process(l, buffer.getWritePointer(1), n, prm);
    else
    {   // mono: run the stereo chain on a copy, keep the left output
        std::vector<float>& tmp = scratchIn[1];
        std::copy(l, l + n, tmp.data());
        chain.process(l, tmp.data(), n, prm);
    }

    for (int i = 0; i < n; ++i)
    {
        const float b = bypassMix.getNextValue();
        if (b > 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                d[i] = d[i] * (1.0f - b) + scratchIn[ch][(size_t) i] * b;
            }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NFVocalSuiteAudioProcessor::createParameters()
{
    using ID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    for (int s = 0; s < nfvs::kStages; ++s)
    {
        const auto& si = nfvs::kStageInfo[s];
        const juce::String nm = juce::String(si.name).toLowerCase();
        const juce::String cap = juce::String(si.name).substring(0, 1) + nm.substring(1);
        v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ stageId(s, "amt"), 1 }, cap + " Amount",
            juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), si.defAmt, juce::AudioParameterFloatAttributes().withLabel("%")));
        v.push_back(std::make_unique<juce::AudioParameterChoice>(ID{ stageId(s, "mode"), 1 }, cap + " Style",
            juce::StringArray { si.modes[0], si.modes[1], si.modes[2] }, si.defMode));
        v.push_back(std::make_unique<juce::AudioParameterBool>(ID{ stageId(s, "on"), 1 }, cap + " On", true));
        v.push_back(std::make_unique<juce::AudioParameterBool>(ID{ stageId(s, "a"), 1 }, cap + " " + si.toggleA, si.defA));
        v.push_back(std::make_unique<juce::AudioParameterBool>(ID{ stageId(s, "b"), 1 }, cap + " " + si.toggleB, si.defB));
    }
    v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ "output", 1 }, "Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ "lowcut", 1 }, "Low Cut",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.45f), 20.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ "highcut", 1 }, "High Cut",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 10.0f, 0.5f), 20000.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    v.push_back(std::make_unique<juce::AudioParameterBool>(ID{ "bypass", 1 }, "Bypass", false));
    return { v.begin(), v.end() };
}

void NFVocalSuiteAudioProcessor::applyPreset(int index)
{
    if (index < 0 || index >= kNumVSPresets) return;
    const auto& pr = kVSPresets[index];
    auto set = [&](const juce::String& id, float value) {
        if (auto* prm = apvts.getParameter(id))
        {
            prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->convertTo0to1(value)); prm->endChangeGesture();
        }
    };
    for (int s = 0; s < nfvs::kStages; ++s)
    {
        set(stageId(s, "amt"), (float) pr.amt[s]);
        set(stageId(s, "mode"), (float) pr.mode[s]);
        set(stageId(s, "on"), (pr.flags[s] & 1) ? 1.0f : 0.0f);
        set(stageId(s, "a"), (pr.flags[s] & 2) ? 1.0f : 0.0f);
        set(stageId(s, "b"), (pr.flags[s] & 4) ? 1.0f : 0.0f);
    }
    apvts.state.setProperty("presetName", juce::String(pr.name), nullptr);
}

void NFVocalSuiteAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void NFVocalSuiteAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* NFVocalSuiteAudioProcessor::createEditor() { return new NFVocalSuiteAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NFVocalSuiteAudioProcessor(); }
