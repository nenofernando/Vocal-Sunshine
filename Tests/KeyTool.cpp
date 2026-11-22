// Developer tool: feeds a wav file to NF Vocal Sunshine's AUTO key learner and prints what it heard.
// Usage: NFVocalSunshineKeyTool file.wav [maxSeconds]
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: KeyTool file.wav [maxSeconds]\n"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(juce::File(argv[1])));
    if (!rd) { std::printf("cannot read %s\n", argv[1]); return 1; }
    const double sr = rd->sampleRate;
    const juce::int64 total = argc > 2 ? std::min<juce::int64>(rd->lengthInSamples, (juce::int64) (std::atof(argv[2]) * sr)) : rd->lengthInSamples;
    nfvs::Chain ch; ch.prepare(sr);
    nfvs::Params p; p.hAuto = true;
    for (int s = 0; s < nfvs::kStages; ++s) { p.amt[s] = 0.0f; p.mode[s] = 1; p.on[s] = true; }
    juce::AudioBuffer<float> buf(1, 480);
    std::vector<float> l(480), r(480);
    for (juce::int64 pos = 0; pos < total; pos += 480)
    {
        const int n = (int) std::min<juce::int64>(480, total - pos);
        rd->read(&buf, 0, n, pos, true, false);
        for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = i < n ? buf.getSample(0, i) : 0.0f;
        ch.process(l.data(), r.data(), 480, p);
    }
    double h[12]; ch.copyKeyHistogram(h);
    double tot = 0; for (double v : h) tot += v;
    std::printf("key=%d conf=%.2f heard=%.1fs\nhist:", ch.autoKeyNow(), ch.autoConfidenceNow(), ch.autoSecondsNow());
    for (int i = 0; i < 12; ++i) std::printf(" %.4f", tot > 0 ? h[i] / tot : 0.0);
    std::printf("\n");
    return 0;
}
