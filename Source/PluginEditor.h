#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NFVocalSuiteAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NFVocalSuiteAudioProcessorEditor(NFVocalSuiteAudioProcessor&);
    ~NFVocalSuiteAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int kBaseW = 810, kBaseH = 500;

private:
    struct KnobLook final : juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        juce::Colour accent { 0xff4cd07a };
    };

    void timerCallback() override;
    float scale() const { return (float) getWidth() / (float) kBaseW; }
    void selectStage(int s);
    float pv(const juce::String& id) const { return proc.apvts.getRawParameterValue(id)->load(); }
    juce::String sid(const char* suffix) const { return NFVocalSuiteAudioProcessor::stageId(stage, suffix); }
    void toggleBool(const juce::String& id);
    void setChoice(const juce::String& id, int index);
    int presetIndex() const;
    void stepPreset(int dir);
    void showPresetMenu();
    void updateSpectrum();
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawVoices(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawHarmony(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawEq(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void applyEqCurve(int style);
    int eqNodeAt(juce::Point<float> pos) const;
    juce::Point<float> eqNodePos(int band) const;
    void setEqFromPos(int band, juce::Point<float> pos);
    juce::Rectangle<float> eqArea() const { return waveRect().reduced(14.0f, 12.0f); }
    juce::String keyId() const { return "hkey"; }

    // layout (base 810 x 500)
    juce::Rectangle<float> bypassRect() const { return { 538.0f, 16.0f, 84.0f, 21.0f }; }
    juce::Rectangle<float> presetRect() const { return { 635.0f, 16.0f, 157.0f, 21.0f }; }
    juce::Rectangle<float> waveRect() const { return { 30.0f, 66.0f, 750.0f, 134.0f }; }
    juce::Rectangle<float> leftPanel() const { return { 30.0f, 210.0f, 214.0f, 200.0f }; }
    juce::Rectangle<float> rightPanel() const { return { 566.0f, 210.0f, 214.0f, 200.0f }; }
    juce::Rectangle<float> modeRect(int i) const { return { 44.0f, 238.0f + 38.0f * (float) i, 186.0f, 30.0f }; }
    juce::Rectangle<float> switchRow(int row) const { return { 576.0f, 236.0f + 30.0f * (float) row, 194.0f, 24.0f }; }   // 0 module, 1 A, 2 B
    juce::Rectangle<float> railDot(int i) const { return { 405.0f + ((float) i - 5.0f) * 62.0f - 30.0f, 428.0f, 60.0f, 50.0f }; }
    juce::Rectangle<float> prevRect() const { return { 20.0f, 436.0f, 30.0f, 34.0f }; }
    juce::Rectangle<float> nextRect() const { return { 760.0f, 436.0f, 30.0f, 34.0f }; }
    juce::Rectangle<float> harmChip(int col) const { const float cw = (waveRect().getWidth() - 20.0f) / 7.0f; return { waveRect().getX() + 10.0f + cw * (float) col + 5.0f, waveRect().getBottom() - 38.0f, cw - 10.0f, 26.0f }; }
    juce::Rectangle<float> keyRect() const { return { 44.0f, 362.0f, 186.0f, 28.0f }; }

    NFVocalSuiteAudioProcessor& proc;
    KnobLook look, smallLook;
    juce::Slider knob, outKnob, mixKnob, lowKnob, highKnob, atkKnob, relKnob, ratKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt, outAtt, mixAtt, lowAtt, highAtt, atkAtt, relAtt, ratAtt;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image logo;
    int stage = 0;
    bool bypassed = false;
    bool draggingSpread = false;
    int dragBand = -1, selBand = 1;
    void setSpreadFromX(float x);
    float act = 0.0f;
    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder, kSpecCols = 200;
    juce::dsp::FFT fft { kFftOrder };
    std::vector<float> fftWin, fftData;
    float specIn[kSpecCols] = {}, specOut[kSpecCols] = {};
    juce::String presetName;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalSuiteAudioProcessorEditor)
};
