#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "License/VocalSunshineLicenseOverlay.h"

class NFVocalSunshineAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NFVocalSunshineAudioProcessorEditor(NFVocalSunshineAudioProcessor&);
    ~NFVocalSunshineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int kBaseW = 810, kBaseH = 500;
    void showLicencePreview();   // developer tool: shows the activation window without enforcing the licence

private:
    struct KnobLook final : juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        juce::Colour accent { 0xff4cd07a };
        int style = 0;   // big knob only: 0 = metal, 1 = sun, 2 = full moon
    };

    void timerCallback() override;
    float scale() const { return (float) getWidth() / (float) kBaseW; }
    void selectStage(int s);
    float pv(const juce::String& id) const { return proc.apvts.getRawParameterValue(id)->load(); }
    juce::String sid(const char* suffix) const { return NFVocalSunshineAudioProcessor::stageId(stage, suffix); }
    void toggleBool(const juce::String& id);
    void setChoice(const juce::String& id, int index);
    int presetIndex() const;
    void stepPreset(int dir);
    void showPresetMenu();
    void setupValueTexts();
    void leaveAutoKey();   // switch AUTO off, keeping the key and scale it had found
    void updateSpectrum();
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawVoices(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawHarmony(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawEq(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    void drawDeEss(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool);
    bool eqMatchesCurve(int style) const;
    void applyEqCurve(int style);
    int eqNodeAt(juce::Point<float> pos) const;
    juce::Point<float> eqNodePos(int band) const;
    void setEqFromPos(int band, juce::Point<float> pos);
    juce::Rectangle<float> eqArea() const { return waveRect().reduced(14.0f, 12.0f); }
    juce::String keyId() const { return "hkey"; }

    // layout (base 810 x 500)
    juce::Rectangle<float> bypassRect() const { return { 538.0f, 16.0f, 84.0f, 21.0f }; }
    juce::Rectangle<float> presetRect() const { return { 635.0f, 16.0f, 157.0f, 21.0f }; }
    juce::Rectangle<float> waveRect() const { return { 180.0f, 66.0f, 612.0f, 148.0f }; }
    juce::Rectangle<float> moduleRow(int i) const { return { 20.0f, 71.0f + 31.5f * (float) i, 146.0f, 28.0f }; }   // left module list
    int segCount() const { return stage == nfvs::Delay ? 6 : 3; }   // DELAY has six time divisions instead of three styles
    juce::Rectangle<float> modeRect(int i) const { return stage == nfvs::Delay ? juce::Rectangle<float>(182.0f + 60.5f * (float) i, 227.0f, 58.0f, 28.0f) : juce::Rectangle<float>(182.0f + 122.0f * (float) i, 227.0f, 118.0f, 28.0f); }   // STYLE segments
    juce::Rectangle<float> switchRow(int row) const { return row == 1 ? juce::Rectangle<float>(552.0f, 226.0f, 118.0f, 30.0f) : row == 2 ? juce::Rectangle<float>(674.0f, 226.0f, 118.0f, 30.0f) : juce::Rectangle<float>(); }   // option buttons A / B
    juce::Rectangle<float> harmChip(int col) const { const float cw = (waveRect().getWidth() - 20.0f) / 7.0f; return { waveRect().getX() + 10.0f + cw * (float) col + 5.0f, waveRect().getBottom() - 38.0f, cw - 10.0f, 26.0f }; }
    juce::Rectangle<float> knobStyleRect() const { return { 452.0f, 16.0f, 78.0f, 21.0f }; }
    juce::Rectangle<float> autoRect() const { return { 552.0f, 292.0f, 74.0f, 34.0f }; }      // HARMONY: AUTO key button
    juce::Rectangle<float> keyRect() const { return { 632.0f, 292.0f, 100.0f, 34.0f }; }      // current key (click = choose by hand)
    juce::Rectangle<float> resetRect() const { return { 738.0f, 292.0f, 54.0f, 34.0f }; }     // forget what AUTO heard
    bool useNotes() const { return true; }   // one reading only (the one that fits the sung notes); the song reading is no longer offered
    void setBoolParam(const juce::String& id, bool v);

    NFVocalSunshineAudioProcessor& proc;
    KnobLook look, smallLook;
    juce::Slider knob, outKnob, mixKnob, lowKnob, highKnob, atkKnob, relKnob, ratKnob, deFreqKnob, deMaxKnob, lofiKnob, fbKnob, formantKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt, outAtt, mixAtt, lowAtt, highAtt, atkAtt, relAtt, ratAtt, deFreqAtt, deMaxAtt, lofiAtt, fbAtt, formantAtt;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image logo, logoScaled, brushed;
    std::unique_ptr<VocalSunshineLicenseOverlay> licenceOverlay;
    void makeLicenceOverlay(bool visible);
    int stage = 0;
    bool bypassed = false;
    bool draggingSpread = false, draggingDeFreq = false;
    void setDeFreqFromX(float x);
    unsigned lastBlocks = 0; int idleTicks = 0; bool idle = false;
    float infoNow(int s) const { return idle ? 0.0f : proc.chainRef().infoAt(s); }
    int dragBand = -1, selBand = 1;
    void setSpreadFromX(float x);
    float act = 0.0f;
    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder, kSpecCols = 200;
    juce::dsp::FFT fft { kFftOrder };
    std::vector<float> fftWin, fftData;
    float specIn[kSpecCols] = {}, specOut[kSpecCols] = {};
    juce::String presetName;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalSunshineAudioProcessorEditor)
};
