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
    for (int v = 0; v < 6; ++v) hvP[v] = apvts.getRawParameterValue("hv" + juce::String(v + 1));
    hkeyP = apvts.getRawParameterValue("hkey");
    vspreadP = apvts.getRawParameterValue("vspread");
    for (int b = 0; b < 4; ++b)
        for (int k = 0; k < 3; ++k) eqP[b][k] = apvts.getRawParameterValue("eq" + juce::String(b + 1) + "fgq"[k]);
    ctlP[0] = apvts.getRawParameterValue("ctlatk"); ctlP[1] = apvts.getRawParameterValue("ctlrel"); ctlP[2] = apvts.getRawParameterValue("ctlratio");
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
    prm.outputDb = outP->load(); prm.mix = mixP->load(); for (int v = 0; v < 6; ++v) prm.hv[v] = hvP[v]->load() > 0.5f;
    prm.hkey = juce::roundToInt(hkeyP->load());
    prm.vSpread = vspreadP->load();
    for (int b = 0; b < 4; ++b) { prm.eqF[b] = eqP[b][0]->load(); prm.eqG[b] = eqP[b][1]->load(); prm.eqQ[b] = eqP[b][2]->load(); }
    prm.ctlAtk = ctlP[0]->load(); prm.ctlRel = ctlP[1]->load(); prm.ctlRatio = ctlP[2]->load();
    prm.lowcutHz = lowP->load(); prm.highcutHz = highP->load();
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
    {
        auto logRange = [](float lo, float hi) { juce::NormalisableRange<float> r(lo, hi, 0.0f); r.setSkewForCentre(std::sqrt(lo * hi)); return r; };
        const float lo[4] = { 40.0f, 120.0f, 800.0f, 3000.0f }, hi[4] = { 500.0f, 2500.0f, 9000.0f, 18000.0f }, def[4] = { 120.0f, 400.0f, 3000.0f, 10000.0f };
        static const char* kBand[4] = { "Low Shelf", "Bell 1", "Bell 2", "High Shelf" };
        for (int b = 0; b < 4; ++b)
        {
            const juce::String n = "eq" + juce::String(b + 1);
            v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ n + "f", 1 }, juce::String("EQ ") + kBand[b] + " Freq", logRange(lo[b], hi[b]), def[b], juce::AudioParameterFloatAttributes().withLabel("Hz")));
            v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ n + "g", 1 }, juce::String("EQ ") + kBand[b] + " Gain", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
            v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ n + "q", 1 }, juce::String("EQ ") + kBand[b] + " Q", logRange(0.3f, 8.0f), 1.0f));
        }
    }
    v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ "vspread", 1 }, "Voices Spread", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
                                                            juce::AudioParameterFloatAttributes().withLabel("%")));
    static const char* kCt[3][2] = { { "ctlatk", "Compressor Attack" }, { "ctlrel", "Compressor Release" }, { "ctlratio", "Compressor Ratio" } };
    for (auto& c : kCt)
        v.push_back(std::make_unique<juce::AudioParameterFloat>(ID{ c[0], 1 }, c[1], juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    static const char* kIv[6] = { "Harmony -8ve", "Harmony -3rd", "Harmony +3rd", "Harmony +5th", "Harmony +6th", "Harmony +8ve" };
    for (int i = 0; i < 6; ++i)
        v.push_back(std::make_unique<juce::AudioParameterBool>(ID{ "hv" + juce::String(i + 1), 1 }, kIv[i], i == 2 || i == 3));
    v.push_back(std::make_unique<juce::AudioParameterChoice>(ID{ "hkey", 1 }, "Harmony Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
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
