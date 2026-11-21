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
        auto p = flat(); p.amt[nfvs::Clean] = 100; p.mode[nfvs::Clean] = 2;
        auto noise = [&](double) { return (float) (0.002 * (nr.nextFloat() - 0.5)); };   // about -60 dBFS
        const auto off = run(flat(), 2.0, noise), on = run(p, 2.0, noise);
        check(dbOf(on.rms) < dbOf(off.rms) - 6.0, "denoise lowers a -60 dB hiss by > 6 dB");
        const auto v0 = run(flat(), 2.0, voice), v1 = run(p, 2.0, voice);
        check(dbOf(v1.rms) > dbOf(v0.rms) - 1.0, "loud voice loses < 1 dB");
    }
    std::printf("FIX: harsh S reduced\n");
    {
        auto p = flat(); p.amt[nfvs::Fix] = 100; p.mode[nfvs::Fix] = 2;
        auto s = [&](double t) { const double burst = std::fmod(t, 0.5) < 0.12 ? 1.0 : 0.0; return (float) (0.3 * (0.5 + 0.5 * std::sin(tw * 2.0 * t)) * std::sin(tw * 200 * t) + 0.12 * burst * std::sin(tw * 7500 * t)); };
        const auto a = run(flat(), 3.0, s), b = run(p, 3.0, s, 0.3, nfvs::Fix);
        check(dbOf(b.rms) < dbOf(a.rms) - 0.3 && b.grMax > 1.0, "de-esser cuts a sibilant burst");
    }
    std::printf("TONE: bright style lifts the highs\n");
    {
        auto p = flat(); p.amt[nfvs::Tone] = 100; p.mode[nfvs::Tone] = 2;
        auto hi = [&](double t) { return (float) (0.1 * std::sin(tw * 11000 * t)); };
        const auto a = run(flat(), 1.0, hi), b = run(p, 1.0, hi);
        check(dbOf(b.rms) > dbOf(a.rms) + 3.0, "air shelf adds > 3 dB at 11 kHz");
    }
    std::printf("CONTROL: compresses loud, evens level\n");
    {
        auto p = flat(); p.amt[nfvs::Control] = 100; p.mode[nfvs::Control] = 2; p.a[nfvs::Control] = false; p.b[nfvs::Control] = false;
        auto loud = [&](double t) { return (float) (0.7 * std::sin(tw * 300 * t)); };
        const auto b = run(p, 2.0, loud, 0.3, nfvs::Control);
        check(b.grMax > 6.0, "heavy compression shows > 6 dB of reduction");
        auto p2 = p; p2.b[nfvs::Control] = true; p2.a[nfvs::Control] = true;
        auto quiet = [&](double t) { return (float) (0.03 * std::sin(tw * 300 * t)); };
        const auto q0 = run(flat(), 8.0, quiet, 6.0), q1 = run(p2, 8.0, quiet, 6.0);
        check(dbOf(q1.rms) > dbOf(q0.rms) + 3.0, "level rider lifts a quiet passage");
    }
    std::printf("COLOR: adds harmonics\n");
    {
        auto p = flat(); p.amt[nfvs::Color] = 100; p.mode[nfvs::Color] = 0; p.a[nfvs::Color] = false; p.b[nfvs::Color] = false;
        auto sine = [&](double t) { return (float) (0.5 * std::sin(tw * 300 * t)); };
        const auto a = run(flat(), 1.0, sine), b = run(p, 1.0, sine);
        check(std::abs(dbOf(b.rms) - dbOf(a.rms)) > 0.1, "tape saturation changes the signal");
        check(b.peak < 1.0 && std::isfinite(b.rms), "stays finite and under full scale");
    }
    std::printf("REVERB: tail present\n");
    {
        auto p = flat(); p.amt[nfvs::Reverb] = 100; p.mode[nfvs::Reverb] = 2; p.a[nfvs::Reverb] = false; p.b[nfvs::Reverb] = false;
        auto burst = [&](double t) { return t < 0.1 ? (float) (0.5 * std::sin(tw * 400 * t)) : 0.0f; };
        const auto a = run(flat(), 1.5, burst, 0.5), b = run(p, 1.5, burst, 0.5);
        check(b.rms > a.rms * 3.0 + 1e-6, "tail is audible 0.4 s after the burst");
    }
    std::printf("FINISH: limiter holds the ceiling\n");
    {
        auto p = flat(); p.amt[nfvs::Finish] = 100; p.mode[nfvs::Finish] = 2;
        auto hot = [&](double t) { return (float) (0.9 * std::sin(tw * 250 * t) * (0.6 + 0.4 * std::sin(tw * 3 * t))); };
        const auto b = run(p, 2.0, hot, 0.3, nfvs::Finish), a = run(flat(), 2.0, hot);
        check(b.peak < 0.95, "peak stays below 0.95 with +12 dB of drive");
        check(dbOf(b.rms) > dbOf(a.rms) + 2.0, "and the signal is louder than without it");
    }

    std::printf("DELAY: echo at the tempo\n");
    {
        auto p = flat(); p.amt[nfvs::Delay] = 100; p.mode[nfvs::Delay] = 1;   // 1/4 note at 120 bpm = 0.5 s
        auto burst = [&](double t) { return t < 0.1 ? (float) (0.5 * std::sin(tw * 400 * t)) : 0.0f; };
        const auto a = run(flat(), 1.2, burst, 0.45), b = run(p, 1.2, burst, 0.45);
        check(b.rms > a.rms * 3.0 + 1e-6, "echo is audible at 0.5 s");
    }
    std::printf("VOICES: opens a mono voice into stereo\n");
    {
        auto side = [&](const nfvs::Params& p, int mode) {
            auto q = p; q.mode[nfvs::Voices] = mode;
            nfvs::Chain ch; ch.prepare(kFs);
            std::vector<float> l(480), r(480); double sd = 0, md = 0;
            for (long blk = 0; blk < 300; ++blk)
            {
                for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.3 * std::sin(tw * 500 * (double) (blk * 480 + i) / kFs) * (0.6 + 0.4 * std::sin(tw * 3 * (double) (blk * 480 + i) / kFs)));
                ch.process(l.data(), r.data(), 480, q);
                if (blk > 60) for (int i = 0; i < 480; ++i) { const double s = 0.5 * (l[(size_t) i] - r[(size_t) i]), m = 0.5 * (l[(size_t) i] + r[(size_t) i]); sd += s * s; md += m * m; }
            }
            return std::sqrt(sd / (md + 1e-12));
        };
        auto p = flat(); p.amt[nfvs::Voices] = 100; p.a[nfvs::Voices] = false; p.b[nfvs::Voices] = false;
        auto off = flat();
        check(side(off, 0) < 1e-4, "no side signal when the module is at 0");
        check(side(p, 0) > 0.15, "DOUBLE adds side energy");
        { auto q = p; q.vSpread = 0.0f; check(side(q, 0) < 0.02, "spread 0 % keeps the doubles in the centre"); q.vSpread = 50.0f; const double half = side(q, 0); check(half > 0.03 && half < side(p, 0), "spread 50 % is between centre and full width"); }
        check(side(p, 1) > 0.15, "STACK adds side energy");
        check(side(p, 2) > 0.15, "WIDE adds side energy");
    }
    std::printf("LOW CUT / HIGH CUT\n");
    {
        auto p = flat(); p.lowcutHz = 300.0f; p.highcutHz = 2000.0f;
        auto lo = [&](double t) { return (float) (0.3 * std::sin(tw * 60 * t)); };
        auto hi = [&](double t) { return (float) (0.3 * std::sin(tw * 9000 * t)); };
        auto mid = [&](double t) { return (float) (0.3 * std::sin(tw * 900 * t)); };
        check(dbOf(run(p, 1.0, lo).rms) < dbOf(run(flat(), 1.0, lo).rms) - 20.0, "low cut removes 60 Hz (> 20 dB)");
        check(dbOf(run(p, 1.0, hi).rms) < dbOf(run(flat(), 1.0, hi).rms) - 20.0, "high cut removes 9 kHz (> 20 dB)");
        check(std::abs(dbOf(run(p, 1.0, mid).rms) - dbOf(run(flat(), 1.0, mid).rms)) < 1.5, "900 Hz passes");
    }

    std::printf("HARMONY: pitch tracker and intervals\n");
    {
        auto goertzel = [&](const nfvs::Params& p, double f0, double fProbe, float* leadOut) {
            nfvs::Chain ch; ch.prepare(kFs);
            std::vector<float> l(480), r(480); double re = 0, im = 0; long n = 0;
            for (long blk = 0; blk < 300; ++blk)
            {
                for (int i = 0; i < 480; ++i) { const double t = (double) (blk * 480 + i) / kFs; l[(size_t) i] = r[(size_t) i] = (float) (0.3 * (std::sin(tw * f0 * t) + 0.3 * std::sin(tw * 2 * f0 * t) + 0.15 * std::sin(tw * 3 * f0 * t))); }
                ch.process(l.data(), r.data(), 480, p);
                if (blk > 100) for (int i = 0; i < 480; ++i) { const double t = (double) (blk * 480 + i) / kFs; re += l[(size_t) i] * std::cos(tw * fProbe * t); im += l[(size_t) i] * std::sin(tw * fProbe * t); ++n; }
            }
            if (leadOut) *leadOut = ch.leadMidiNow();
            return 20.0 * std::log10(std::sqrt(re * re + im * im) / (double) n * 2.0 + 1e-9);
        };
        auto hp = flat(); hp.amt[nfvs::Harmony] = 100; hp.mode[nfvs::Harmony] = 2; hp.a[nfvs::Harmony] = false; hp.b[nfvs::Harmony] = false;
        float lead = -1.0f;
        auto dn = hp; dn.hv[0] = true;                         // -8ve: 110 Hz is not in the test signal
        const double with = goertzel(dn, 220.0, 110.0, &lead), without = goertzel(flat(), 220.0, 110.0, nullptr);
        check(std::abs(lead - 57.0f) < 0.6f, "pitch tracker reads 220 Hz as A3 (MIDI 57)");
        check(with > without + 10.0, "-8ve voice adds energy one octave down (> 10 dB at 110 Hz)");
        auto fifth = hp; fifth.hv[3] = true;                  // +5th, FIXED = +7 semitones = 329.6 Hz
        check(goertzel(fifth, 220.0, 329.63, nullptr) > goertzel(flat(), 220.0, 329.63, nullptr) + 6.0, "+5th voice (fixed) lands on 329.6 Hz");
        auto maj = hp; maj.mode[nfvs::Harmony] = 0; maj.hv[2] = true; maj.hkey = 9;   // A major, lead A3: third above = C#4 (277.2 Hz)
        check(goertzel(maj, 220.0, 277.18, nullptr) > goertzel(flat(), 220.0, 277.18, nullptr) + 6.0, "key-aware +3rd of A in A major = C#");
        auto mnr = maj; mnr.mode[nfvs::Harmony] = 1;                      // A minor: third above = C (261.6 Hz)
        check(goertzel(mnr, 220.0, 261.63, nullptr) > goertzel(flat(), 220.0, 261.63, nullptr) + 6.0, "in A minor the +3rd is C");
    }

    std::printf("PITCH: correction onto the scale\n");
    {
        auto goertz = [&](const nfvs::Params& p, double f0, double fProbe) {
            nfvs::Chain ch; ch.prepare(kFs);
            std::vector<float> l(480), r(480); double re = 0, im = 0; long n = 0;
            for (long blk = 0; blk < 400; ++blk)
            {
                for (int i = 0; i < 480; ++i) { const double t = (double) (blk * 480 + i) / kFs; l[(size_t) i] = r[(size_t) i] = (float) (0.3 * (std::sin(tw * f0 * t) + 0.3 * std::sin(tw * 2 * f0 * t) + 0.15 * std::sin(tw * 3 * f0 * t))); }
                ch.process(l.data(), r.data(), 480, p);
                if (blk > 160) for (int i = 0; i < 480; ++i) { const double t = (double) (blk * 480 + i) / kFs; re += l[(size_t) i] * std::cos(tw * fProbe * t); im += l[(size_t) i] * std::sin(tw * fProbe * t); ++n; }
            }
            return 20.0 * std::log10(std::sqrt(re * re + im * im) / (double) n * 2.0 + 1e-9);
        };
        const double sharp = 220.0 * std::pow(2.0, 0.35 / 12.0);      // 35 cents above A3
        auto p = flat(); p.amt[nfvs::Pitch] = 100; p.mode[nfvs::Pitch] = 2; p.a[nfvs::Pitch] = true; p.b[nfvs::Pitch] = false;   // chromatic, hard
        check(goertz(p, sharp, 220.0) > goertz(flat(), sharp, 220.0) + 12.0, "a note 35 cents sharp is pulled onto 220 Hz");
        auto half = p; half.amt[nfvs::Pitch] = 0;
        check(std::abs(goertz(half, sharp, sharp) - goertz(flat(), sharp, sharp)) < 0.5, "amount 0 leaves the note alone");
        auto maj = p; maj.mode[nfvs::Pitch] = 0; maj.pkey = 0;          // C major: 233.1 Hz (A#3) is not in the scale, nearest are A3 and B3
        const double ais = 220.0 * std::pow(2.0, 1.0 / 12.0 + 0.2 / 12.0);
        check(goertz(maj, ais, 246.94) > goertz(flat(), ais, 246.94) + 10.0, "C major pulls A#3 (+20 cents) up to B3");
    }
    std::printf("EQ: shelves and bells\n");
    {
        auto tone = [&](double f) { return [f, tw](double t) { return (float) (0.2 * std::sin(tw * f * t)); }; };
        auto p = flat(); p.amt[nfvs::Eq] = 100;
        auto lowUp = p; lowUp.eqG[0] = 12.0f; lowUp.eqF[0] = 150.0f;
        check(dbOf(run(lowUp, 1.0, tone(50.0)).rms) > dbOf(run(flat(), 1.0, tone(50.0)).rms) + 8.0, "low shelf +12 dB lifts 50 Hz");
        auto bellDown = p; bellDown.eqG[2] = -12.0f; bellDown.eqF[2] = 3000.0f; bellDown.eqQ[2] = 1.0f;
        check(dbOf(run(bellDown, 1.0, tone(3000.0)).rms) < dbOf(run(flat(), 1.0, tone(3000.0)).rms) - 8.0, "bell -12 dB cuts 3 kHz");
        check(std::abs(dbOf(run(bellDown, 1.0, tone(300.0)).rms) - dbOf(run(flat(), 1.0, tone(300.0)).rms)) < 1.0, "and leaves 300 Hz alone");
        auto hiUp = p; hiUp.eqG[3] = 10.0f; hiUp.eqF[3] = 6000.0f;
        check(dbOf(run(hiUp, 1.0, tone(12000.0)).rms) > dbOf(run(flat(), 1.0, tone(12000.0)).rms) + 6.0, "high shelf +10 dB lifts 12 kHz");
        auto off = lowUp; off.amt[nfvs::Eq] = 0;
        check(std::abs(dbOf(run(off, 1.0, tone(50.0)).rms) - dbOf(run(flat(), 1.0, tone(50.0)).rms)) < 0.05, "EQ amount 0 is transparent");
    }

    std::printf(bad ? "FAIL (%d problems)\n" : "OK\n", bad);
    return bad ? 1 : 0;
}
