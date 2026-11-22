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
    nfvs::Params p; p.hAuto = true; p.amt[nfvs::Harmony] = 100.0f; p.mode[nfvs::Harmony] = 0; p.hv[2] = true; p.hLead = true;   // +3rd (major scale when AUTO says major, minor otherwise)
    for (int s = 0; s < nfvs::kStages; ++s) { if (s != nfvs::Harmony) p.amt[s] = 0.0f; p.mode[s] = s == nfvs::Harmony ? 0 : 1; p.on[s] = true; }
    juce::AudioBuffer<float> buf(1, 480);
    long shVoiced = 0, shBetween = 0, shChanges = 0; float prevSh = -99.0f;
    long frames = 0, inSong = 0, inPhrase = 0, inFinal = 0, inOld = 0, inOldFinal = 0;
    // the previous learner (pitch-tracker histogram, half-life about a minute), re-created here only to compare
    double oldChroma[12] = {}; int oldKey = -1, oldSeen = 0, oldCount = 0; double oldSecs = 0;
    auto oldEstimate = [&]() {
        static const double maj[] = { 17.7661, 0.145624, 14.9265, 0.160186, 19.8049, 11.3587, 0.291248, 22.062, 0.145624, 8.15494, 0.232998, 4.95122 };
        static const double mnr[] = { 18.2648, 0.737619, 14.0502, 16.8599, 0.702494, 14.4362, 0.702494, 18.6161, 4.56621, 1.93186, 7.37619, 1.75623 };
        auto corr = [&](const double* prof, int shift) {
            double mx = 0, mp = 0; for (int i = 0; i < 12; ++i) { mx += oldChroma[i]; mp += prof[i]; } mx /= 12; mp /= 12;
            double num = 0, dx = 0, dp = 0; for (int i = 0; i < 12; ++i) { const double a = oldChroma[(i + shift) % 12] - mx, b = prof[i] - mp; num += a * b; dx += a * a; dp += b * b; }
            return dx > 0 && dp > 0 ? num / std::sqrt(dx * dp) : 0.0; };
        double best = -2; int bk = -1; for (int t = 0; t < 12; ++t) for (int m = 0; m < 2; ++m) { const double c = corr(m ? mnr : maj, t); if (c > best) { best = c; bk = t + 12 * m; } }
        oldKey = bk; };
    std::vector<std::pair<float,std::vector<int>>> log;   // midi, (song key, phrase key, old key) at that moment
    auto inScale = [](int key, int pc) { if (key < 0) return true; static const bool mj[12] = {1,0,1,0,1,1,0,1,0,1,0,1}, mn[12] = {1,0,1,1,0,1,0,1,1,0,1,0}; const int rel = ((pc - key % 12) % 12 + 12) % 12; return key >= 12 ? mn[rel] : mj[rel]; };
    std::vector<float> l(480), r(480);
    for (juce::int64 pos = 0; pos < total; pos += 480)
    {
        const int n = (int) std::min<juce::int64>(480, total - pos);
        rd->read(&buf, 0, n, pos, true, false);
        for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = i < n ? buf.getSample(0, i) : 0.0f;
        ch.process(l.data(), r.data(), 480, p);
        {   // how stable is the interval of the +3rd voice?
            const float sh = ch.harmonySemitonesNow(2);
            if (ch.leadVoicedNow()) { ++shVoiced; if (std::abs(sh - std::round(sh)) > 0.2f) ++shBetween; if (prevSh > -90.0f && std::abs(std::round(sh) - std::round(prevSh)) >= 1.0f) ++shChanges; prevSh = sh; } else prevSh = -99.0f;
        }
        if (ch.leadFramesRaw() != oldSeen)
        {
            oldSeen = ch.leadFramesRaw();
            if (ch.leadFreshRaw())
            {
                const double m = ch.leadMidiRaw(), rn = std::round(m), dev = std::abs(m - rn);
                for (auto& c : oldChroma) c *= 0.99988;
                oldSecs += 128.0 * 4.0 / sr;
                if (dev <= 0.4) oldChroma[(((int) rn) % 12 + 12) % 12] += 1.0 - 1.2 * dev;
                if (++oldCount % 40 == 0 && oldSecs >= 3.0) oldEstimate();
            }
        }
        if (ch.leadVoicedNow() && ch.autoKeyFor(false) >= 0 && oldKey >= 0) log.push_back({ ch.leadMidiRaw(), { ch.autoKeyFor(false), ch.autoKeyFor(true), oldKey } });
    }
    const int finalKey = ch.autoKeyFor(false);
    for (auto& e : log)
    {
        const int pc = (((int) std::lround(e.first)) % 12 + 12) % 12;
        ++frames; inSong += inScale(e.second[0], pc); inPhrase += inScale(e.second[1], pc); inFinal += inScale(finalKey, pc); inOld += inScale(e.second[2], pc); inOldFinal += inScale(oldKey, pc);
    }
    std::printf("+3rd voice: %.1f interval changes per second of singing, %.1f %% of the time between two semitones (gliding)\n", shVoiced ? (double) shChanges / ((double) shVoiced * 480.0 / sr) : 0.0, shVoiced ? 100.0 * (double) shBetween / (double) shVoiced : 0.0);
    auto pc100 = [&](long v) { return frames ? 100.0 * (double) v / (double) frames : 0.0; };
    std::printf("notes in the scale (%ld frames): OLD learner %.1f | NEW song %.1f | NEW notes %.1f | (final song key fixed %.1f)\n", frames, pc100(inOld), pc100(inSong), pc100(inPhrase), pc100(inFinal));
    double h[12]; ch.copyKeyHistogram(h);
    double tot = 0; for (double v : h) tot += v;
    std::printf("key=%d conf=%.2f heard=%.1fs\nhist:", ch.autoKeyNow(), ch.autoConfidenceNow(), ch.autoSecondsNow());
    for (int i = 0; i < 12; ++i) std::printf(" %.4f", tot > 0 ? h[i] / tot : 0.0);
    std::printf("\n");
    return 0;
}
