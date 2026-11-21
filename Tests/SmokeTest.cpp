// 1) every factory preset on a synthetic voice: no NaN, no overs.  2) each stage does what it claims (DSP checks).
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"

namespace
{
constexpr double kFs = 48000.0;
int bad = 0;
void check(bool ok, const char* what) { std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what); if (!ok) ++bad; }

nfvs::Params flat()
{
    nfvs::Params p;
    for (int s = 0; s < nfvs::kStages; ++s) { p.amt[s] = 0.0f; p.mode[s] = 1; p.on[s] = true; p.a[s] = true; p.b[s] = true; }
    return p;
}

// runs `secs` of a generator through a fresh chain; returns rms / peak of the processed left channel after `skip` seconds
struct Result { double rms = 0, peak = 0, grMax = 0, inRms = 0; };
template <class Gen>
Result run(const nfvs::Params& p, double secs, Gen gen, double skip = 0.3, int info = -1)
{
    nfvs::Chain ch; ch.prepare(kFs);
    Result r; long cnt = 0; double sum = 0, sumIn = 0;
    std::vector<float> l(480), rr(480);
    for (long blk = 0; blk < (long) (secs * kFs / 480); ++blk)
    {
        for (int i = 0; i < 480; ++i) l[(size_t) i] = rr[(size_t) i] = gen((double) (blk * 480 + i) / kFs);
        const auto in = l;
        ch.process(l.data(), rr.data(), 480, p);
        if ((double) blk * 480 / kFs >= skip)
            for (int i = 0; i < 480; ++i) { sum += (double) l[(size_t) i] * l[(size_t) i]; sumIn += (double) in[(size_t) i] * in[(size_t) i]; ++cnt; r.peak = std::max(r.peak, (double) std::abs(l[(size_t) i])); }
        if (info >= 0) r.grMax = std::max(r.grMax, (double) ch.infoAt(info));
    }
    r.rms = std::sqrt(sum / (double) std::max(1L, cnt)); r.inRms = std::sqrt(sumIn / (double) std::max(1L, cnt));
    return r;
}
double dbOf(double v) { return 20.0 * std::log10(v + 1e-12); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf("Presets\n");
    NFVocalSuiteAudioProcessor proc;
    proc.setPlayConfigDetails(2, 2, kFs, 480);
    proc.prepareToPlay(kFs, 480);
    juce::Random rng(7);
    for (int pi = 0; pi < kNumVSPresets; ++pi)
    {
        proc.applyPreset(pi);
        float maxOut = 0.0f; int nan = 0; double phase = 0.0;
        for (int blk = 0; blk < 400; ++blk)
        {
            juce::AudioBuffer<float> b(2, 480);
            for (int i = 0; i < 480; ++i)
            {
                const double env = 0.5 + 0.5 * std::sin((blk * 480 + i) * 2.0 * juce::MathConstants<double>::pi * 3.0 / kFs);
                phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / kFs;
                const float v = (float) (0.4 * env * (std::sin(phase) + 0.4 * std::sin(3 * phase)) + 0.08 * (rng.nextFloat() - 0.5) * (blk % 20 > 15 ? 4.0 : 1.0));
                b.setSample(0, i, v); b.setSample(1, i, v);
            }
            juce::MidiBuffer mb; proc.processBlock(b, mb);
            for (int i = 0; i < 480; ++i) { const float o = b.getSample(0, i); maxOut = std::max(maxOut, std::abs(o)); if (!std::isfinite(o)) ++nan; }
        }
        std::printf("  %-13s peak %.3f\n", kVSPresets[pi].name, maxOut);
        if (maxOut > 1.0f || nan) { std::printf("  [FAIL] %s\n", kVSPresets[pi].name); ++bad; }
    }

    const double tw = 2.0 * juce::MathConstants<double>::pi;
    juce::Random nr(11);
    auto voice = [&](double t) { const double syl = 0.5 + 0.5 * std::sin(t * tw * 2.5); return (float) (0.3 * syl * (std::sin(tw * 200 * t) + 0.4 * std::sin(tw * 600 * t))); };

    std::printf("Transparent when everything is off\n");
    {
        auto p = flat();
        for (int s = 0; s < nfvs::kStages; ++s) p.on[s] = false;
        const auto r = run(p, 2.0, voice);
        check(std::abs(dbOf(r.rms) - dbOf(r.inRms)) < 0.05, "level unchanged (< 0.05 dB)");
    }
    std::printf("CLEAN: noise down, voice kept\n");
    {
        auto p = flat(); p.amt[0] = 100; p.mode[0] = 2;
        auto noise = [&](double) { return (float) (0.002 * (nr.nextFloat() - 0.5)); };   // about -60 dBFS
        const auto off = run(flat(), 2.0, noise), on = run(p, 2.0, noise);
        check(dbOf(on.rms) < dbOf(off.rms) - 6.0, "denoise lowers a -60 dB hiss by > 6 dB");
        const auto v0 = run(flat(), 2.0, voice), v1 = run(p, 2.0, voice);
        check(dbOf(v1.rms) > dbOf(v0.rms) - 1.0, "loud voice loses < 1 dB");
    }
    std::printf("FIX: harsh S reduced\n");
    {
        auto p = flat(); p.amt[1] = 100; p.mode[1] = 2;
        auto s = [&](double t) { const double burst = std::fmod(t, 0.5) < 0.12 ? 1.0 : 0.0; return (float) (0.3 * (0.5 + 0.5 * std::sin(tw * 2.0 * t)) * std::sin(tw * 200 * t) + 0.12 * burst * std::sin(tw * 7500 * t)); };
        const auto a = run(flat(), 3.0, s), b = run(p, 3.0, s, 0.3, 1);
        check(dbOf(b.rms) < dbOf(a.rms) - 0.3 && b.grMax > 1.0, "de-esser cuts a sibilant burst");
    }
    std::printf("TONE: bright style lifts the highs\n");
    {
        auto p = flat(); p.amt[2] = 100; p.mode[2] = 2;
        auto hi = [&](double t) { return (float) (0.1 * std::sin(tw * 11000 * t)); };
        const auto a = run(flat(), 1.0, hi), b = run(p, 1.0, hi);
        check(dbOf(b.rms) > dbOf(a.rms) + 3.0, "air shelf adds > 3 dB at 11 kHz");
    }
    std::printf("CONTROL: compresses loud, evens level\n");
    {
        auto p = flat(); p.amt[3] = 100; p.mode[3] = 2; p.a[3] = false; p.b[3] = false;
        auto loud = [&](double t) { return (float) (0.7 * std::sin(tw * 300 * t)); };
        const auto b = run(p, 2.0, loud, 0.3, 3);
        check(b.grMax > 6.0, "heavy compression shows > 6 dB of reduction");
        auto p2 = p; p2.b[3] = true; p2.a[3] = true;
        auto quiet = [&](double t) { return (float) (0.03 * std::sin(tw * 300 * t)); };
        const auto q0 = run(flat(), 8.0, quiet, 6.0), q1 = run(p2, 8.0, quiet, 6.0);
        check(dbOf(q1.rms) > dbOf(q0.rms) + 3.0, "level rider lifts a quiet passage");
    }
    std::printf("COLOR: adds harmonics\n");
    {
        auto p = flat(); p.amt[4] = 100; p.mode[4] = 0; p.a[4] = false; p.b[4] = false;
        auto sine = [&](double t) { return (float) (0.5 * std::sin(tw * 300 * t)); };
        const auto a = run(flat(), 1.0, sine), b = run(p, 1.0, sine);
        check(std::abs(dbOf(b.rms) - dbOf(a.rms)) > 0.1, "tape saturation changes the signal");
        check(b.peak < 1.0 && std::isfinite(b.rms), "stays finite and under full scale");
    }
    std::printf("SPACE: reverb tail present\n");
    {
        auto p = flat(); p.amt[5] = 100; p.mode[5] = 2; p.a[5] = false; p.b[5] = false;
        auto burst = [&](double t) { return t < 0.1 ? (float) (0.5 * std::sin(tw * 400 * t)) : 0.0f; };
        const auto a = run(flat(), 1.5, burst, 0.5), b = run(p, 1.5, burst, 0.5);
        check(b.rms > a.rms * 3.0 + 1e-6, "tail is audible 0.4 s after the burst");
    }
    std::printf("FINISH: limiter holds the ceiling\n");
    {
        auto p = flat(); p.amt[6] = 100; p.mode[6] = 2;
        auto hot = [&](double t) { return (float) (0.9 * std::sin(tw * 250 * t) * (0.6 + 0.4 * std::sin(tw * 3 * t))); };
        const auto b = run(p, 2.0, hot, 0.3, 6), a = run(flat(), 2.0, hot);
        check(b.peak < 0.95, "peak stays below 0.95 with +12 dB of drive");
        check(dbOf(b.rms) > dbOf(a.rms) + 2.0, "and the signal is louder than without it");
    }

    std::printf(bad ? "FAIL (%d problems)\n" : "OK\n", bad);
    return bad ? 1 : 0;
}
