// Developer tool: renders NF Vocal Suite's editor to a PNG. Usage: NFVocalSuiteSnapshot out.png [stage 0..6] [scale] [preset]
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    NFVocalSuiteAudioProcessor proc;
    proc.applyPreset(argc > 4 ? std::atoi(argv[4]) : 1);
    proc.apvts.state.setProperty("stage", argc > 2 ? std::atoi(argv[2]) : 0, nullptr);
    proc.apvts.state.setProperty("knobStyle", argc > 5 ? std::atoi(argv[5]) : 0, nullptr);
    proc.setPlayConfigDetails(2, 2, 48000.0, 480);
    proc.prepareToPlay(48000.0, 480);

    // feed ~5 s of a voice-like signal so the waveform and meters have something to show
    juce::Random rng(3);
    double phase = 0.0;
    for (int blk = 0; blk < 520; ++blk)
    {
        juce::AudioBuffer<float> b(2, 480);
        for (int i = 0; i < 480; ++i)
        {
            const double t = (blk * 480 + i) / 48000.0;
            const double syl = std::pow(0.5 + 0.5 * std::sin(t * 2.0 * juce::MathConstants<double>::pi * 2.6), 1.6);
            phase += 2.0 * juce::MathConstants<double>::pi * (190.0 + 40.0 * std::sin(t * 3.0)) / 48000.0;
            const float v = (float) (0.55 * syl * (std::sin(phase) + 0.5 * std::sin(2 * phase) + 0.3 * std::sin(5 * phase)) * (0.6 + 0.4 * std::sin(t * 0.9))
                                     + 0.01 * (rng.nextFloat() - 0.5));
            b.setSample(0, i, v); b.setSample(1, i, v);
        }
        juce::MidiBuffer mb; proc.processBlock(b, mb);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(810, 500);
    for (int i = 0; i < 80; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, argc > 3 ? (float) std::atof(argv[3]) : 1.5f);
    juce::File out(argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os(out);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
