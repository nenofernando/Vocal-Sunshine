#pragma once
#include <JuceHeader.h>
#include "SuiteDSP.h"

class NFVocalSunshineAudioProcessor final : public juce::AudioProcessor
{
public:
    NFVocalSunshineAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    static juce::String stageId(int stage, const char* suffix) { return "s" + juce::String(stage + 1) + suffix; }
    void applyPreset(int index);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<unsigned> blockCount { 0 };   // incremented by every processBlock call (the editor uses it to tell when audio stopped)
    const nfvs::Chain& chainRef() const { return chain; }

private:
    nfvs::Chain chain;
    std::atomic<float>* amtP[nfvs::kStages] = {}; std::atomic<float>* modeP[nfvs::kStages] = {}; std::atomic<float>* onP[nfvs::kStages] = {};
    std::atomic<float>* aP[nfvs::kStages] = {}; std::atomic<float>* bP[nfvs::kStages] = {};
    std::atomic<float>* outP = nullptr; std::atomic<float>* mixP = nullptr; std::atomic<float>* hvP[6] = {}; std::atomic<float>* hkeyP = nullptr; std::atomic<float>* hleadP = nullptr; std::atomic<float>* deP[2] = {}; std::atomic<float>* lofiP = nullptr; std::atomic<float>* stP[nfvs::kStages][4] = {}; std::atomic<float>* ctlP[3] = {}; std::atomic<float>* vspreadP = nullptr; std::atomic<float>* eqP[4][3] = {}; std::atomic<float>* lowP = nullptr; std::atomic<float>* highP = nullptr; std::atomic<float>* bypassP = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
    std::vector<float> scratchIn[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalSunshineAudioProcessor)
};
