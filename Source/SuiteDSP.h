#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include "Stages.h"

// NF Vocal Sunshine signal chain: CLEAN -> FIX -> TONE -> CONTROL -> COLOR -> SPACE -> FINISH.
// Each stage has an amount (0..1), a style (0..2), a module switch and two option switches.
// Zero latency, no allocation while processing. A stage that is off or at 0 is transparent.
namespace nfvs
{
struct Params
{
    float amt[kStages] = {};          // 0..100
    int mode[kStages] = {};           // 0..2
    bool on[kStages] = {}, a[kStages] = {}, b[kStages] = {};
    float outputDb = 0.0f, mix = 100.0f;
    float eqF[4] = { 120.0f, 400.0f, 3000.0f, 10000.0f }, eqG[4] = {}, eqQ[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // EQ: low shelf, 2 bells, high shelf
    bool hLead = true;                // HARMONY: false = mute the lead and keep only the interval voices
    bool hv[6] = {};                  // harmony intervals: -8ve, -3rd, +3rd, +5th, +6th, +8ve
    int hkey = 0;                     // harmony key, 0 = C
    float vSpread = 100.0f;           // VOICES: how far the left / right copies open, 0..100 %
    float deFreq = 6500.0f, deMax = 8.0f;   // DE-ESSER: centre frequency (Hz) and maximum cut (dB)
    float ctlAtk = 50.0f, ctlRel = 50.0f, ctlRatio = 50.0f;   // compressor trims, 0..100 (50 = the style's own value)
    float lowcutHz = 20.0f, highcutHz = 20000.0f;   // 20 Hz / 20 kHz = off
    double bpm = 120.0;
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };
    float process(int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void set(double b0_, double b1_, double b2_, double a0, double a1_, double a2_)
    {
        b0 = (float) (b0_ / a0); b1 = (float) (b1_ / a0); b2 = (float) (b2_ / a0);
        a1 = (float) (a1_ / a0); a2 = (float) (a2_ / a0);
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    static double w0(double fs, double f) { return 2.0 * juce::MathConstants<double>::pi * std::min(f, fs * 0.45) / fs; }
    void highPass(double fs, double f, double q)
    {
        const double w = w0(fs, f), c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void lowPass(double fs, double f, double q)
    {
        const double w = w0(fs, f), c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandPass(double fs, double f, double q)   // 0 dB peak gain
    {
        const double w = w0(fs, f), c = std::cos(w), al = std::sin(w) / (2 * q);
        set(al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak(double fs, double f, double q, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0), w = w0(fs, f), c = std::cos(w), al = std::sin(w) / (2 * q);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf(double fs, double f, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0), w = w0(fs, f), c = std::cos(w), s = std::sin(w);
        const double al = s / 2.0 * std::sqrt(2.0), sq = 2.0 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
            (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf(double fs, double f, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0), w = w0(fs, f), c = std::cos(w), s = std::sin(w);
        const double al = s / 2.0 * std::sqrt(2.0), sq = 2.0 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
            (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

inline float softClip(float x) noexcept
{
    const float ax = std::abs(x);
    if (ax < 0.9f) return x;
    return std::copysign(0.9f + 0.1f * std::tanh((ax - 0.9f) * 10.0f), x);
}

inline float toDb(float lin) noexcept { return 20.0f * std::log10(lin + 1e-9f); }
inline float toGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }

// Own pitch tracker: decimated (about 12 kHz) YIN-style difference function every 512 samples, range 70..800 Hz.
struct PitchTracker
{
    double fs = 48000.0;
    int decim = 4, w = 0, since = 0, decCount = 0, unvoicedFrames = 100;
    float decAcc = 0.0f, midi = 60.0f, period = 0.0f;   // midi = last detected note (fractional), period in samples of the original rate
    bool voiced = false;
    float buf[1024] = {};

    void prepare(double sr) { fs = sr; decim = std::max(1, (int) std::lround(fs / 12000.0)); reset(); }
    void reset() { std::fill(std::begin(buf), std::end(buf), 0.0f); w = since = decCount = 0; decAcc = 0.0f; midi = 60.0f; period = 0.0f; voiced = false; unvoicedFrames = 100; }

    void push(float v)
    {
        decAcc += v;
        if (++decCount < decim) return;
        buf[w] = decAcc / (float) decim; w = (w + 1) & 1023; decCount = 0; decAcc = 0.0f;
        if (++since >= 128) { since = 0; detect(); }
    }

    void detect()
    {
        constexpr int W = 512;
        const double fsd = fs / (double) decim;
        const int minTau = std::max(2, (int) (fsd / 800.0)), maxTau = std::min((int) (fsd / 70.0), 1024 - W - 1);
        if (maxTau <= minTau + 2) return;
        float xs[1024];
        double en = 0.0;
        for (int k = 0; k < W + maxTau; ++k) { xs[k] = buf[(w - (W + maxTau) + k) & 1023]; if (k >= maxTau) en += (double) xs[k] * xs[k]; }
        if (std::sqrt(en / W) < 0.004) { if (++unvoicedFrames > 12) voiced = false; return; }
        float d[1024], dn[1024]; d[0] = 0.0f; float run = 0.0f;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j) { const float df = xs[j] - xs[j + tau]; sum += df * df; }
            d[tau] = sum; run += sum; dn[tau] = run > 0.0f ? sum * (float) tau / run : 1.0f;
        }
        int best = -1;
        for (int tau = minTau; tau <= maxTau - 1; ++tau)
            if (dn[tau] < 0.15f) { while (tau + 1 < maxTau && dn[tau + 1] < dn[tau]) ++tau; best = tau; break; }
        if (best < 0) { if (++unvoicedFrames > 12) voiced = false; return; }
        float tauF = (float) best;
        if (best > 1 && best < maxTau) { const float a = dn[best - 1], b = dn[best], c = dn[best + 1], den = a - 2.0f * b + c; if (std::abs(den) > 1e-9f) tauF += 0.5f * (a - c) / den; }
        const float f = (float) (fsd / tauF);
        const float m = 69.0f + 12.0f * std::log2(f / 440.0f);
        midi = (voiced && std::abs(m - midi) < 3.0f) ? midi + (m - midi) * 0.5f : m;
        period = tauF * (float) decim; voiced = true; unvoicedFrames = 0;
    }
};

class Chain
{
public:
    static constexpr int kChunk = 512, kHist = 256, kTaps = 13;   // tap 0 = input, tap k = after stage k

    void prepare(double sampleRate)
    {
        fs = sampleRate;
        delayLen = (int) (fs * 3.2) + 8;
        delayBuf[0].assign((size_t) delayLen, 0.0f);
        delayBuf[1].assign((size_t) delayLen, 0.0f);
        preLen = (int) (fs * 0.06) + 2;
        preBuf.assign((size_t) preLen, 0.0f);
        reverb.setSampleRate(fs);
        sibHp.highPass(fs, 6000.0, 0.707);
        const double bf[3] = { 320.0, 1000.0, 3400.0 }, bq[3] = { 1.4, 1.8, 1.6 };
        for (int b = 0; b < 3; ++b) band[b].bandPass(fs, bf[b], bq[b]);
        vLen = (int) (fs * 0.08) + 4; vBuf.assign((size_t) vLen, 0.0f);
        vHp.highPass(fs, 250.0, 0.707); vLp.lowPass(fs, 5000.0, 0.707); hLp.lowPass(fs, 5000.0, 0.707);
        trkH.prepare(fs); hLen = 0.03f * (float) fs;
        eqKey.fill(-1000.0f);
        exHp.highPass(fs, 4000.0, 0.707);
        thumpLp.lowPass(fs, 140.0, 0.707);
        sendHp.highPass(fs, 220.0, 0.707);
        sendLp.lowPass(fs, 6000.0, 0.707);
        colLen = std::max(1, (int) (fs * 0.02));
        reset();
        primed = false;
        lastKey.fill(-1000.0f);
        for (auto& row : hist) for (auto& v : row) v.store(0.0f, std::memory_order_relaxed);
        histPos.store(0);
        for (auto& v : info) v.store(0.0f, std::memory_order_relaxed);
    }

    void reset()
    {
        for (auto* b : { &deBp, &eqBq[0], &eqBq[1], &eqBq[2], &eqBq[3], &hLp, &lowcut, &lcB, &hcA, &hcB, &vHp, &vLp, &sibHp, &band[0], &band[1], &band[2], &warm, &clarCut, &clarBoost, &pres, &airShelf, &exHp, &thumpLp, &sendHp, &sendLp })
            b->reset();
        reverb.reset();
        std::fill(delayBuf[0].begin(), delayBuf[0].end(), 0.0f);
        std::fill(delayBuf[1].begin(), delayBuf[1].end(), 0.0f);
        std::fill(preBuf.begin(), preBuf.end(), 0.0f);
        envFullD = envDe = 0; gDe = 1.0f; deKey = { -1.0f, -1.0f };
        dw = 0; pw = 0; envFull = envSib = 0; gNoise = gSib = 1.0f; grDb = 0; duck = 0; dlyLp[0] = dlyLp[1] = 0;
        delaySmoothed = -1.0f; riderDb = 0; riderLin = 1.0f; rmsEnv = 0; gLim = 1.0f;
        for (int b = 0; b < 3; ++b) { envB[b] = 0; avgRel[b] = -18.0f; gBand[b] = 1.0f; }
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
        std::fill(vBuf.begin(), vBuf.end(), 0.0f); vw = 0; lfo1 = 0; lfo2 = 1.3f;
        for (int v = 0; v < 6; ++v) { hPhase[v] = 0.0f; hSemi[v] = 0.0f; hPrev[v] = false; }
        trkH.reset(); leadOut.store(-1.0f); leadG = 1.0f;
        colCount = 0; for (auto& v : colPk) v = 0;
    }

    // In place on two channels (r may be a copy of l for mono input).
    void process(float* l, float* r, int n, const Params& p)
    {
        const float outGain = juce::Decibels::decibelsToGain(p.outputDb);
        const float mixAmt = juce::jlimit(0.0f, 1.0f, p.mix / 100.0f);
        static const float kDivMult[3] = { 0.5f, 1.0f, 1.5f };
        const float beat = (float) (60.0 / std::max(40.0, p.bpm));
        const float delayTarget = juce::jlimit(0.02f, 3.0f, beat * kDivMult[juce::jlimit(0, 2, p.mode[Delay])]) * (float) fs;
        if (delaySmoothed < 0.0f) delaySmoothed = delayTarget;

        auto tc = [&](float sec) { return 1.0f - std::exp(-1.0f / (sec * (float) fs)); };
        const float aEnv = tc(0.004f), aNoiseOpen = tc(0.002f), aNoiseClose = tc(0.12f);
        const float aSibAtk = tc(0.0006f), aSibRel = tc(0.05f), aBAtk = tc(0.003f), aBRel = tc(0.06f), aAvg = tc(1.2f);
        const float aRms = tc(0.3f), aRider16 = 1.0f - std::exp(-16.0f / (0.5f * (float) fs));
        const float duckAtk = tc(0.01f), duckRel = tc(0.28f);

        float infoMax[kStages] = {};
        int done = 0;
        while (done < n)
        {
            const int c = std::min(kChunk, n - done);
            float* L = l + done;
            float* R = r + done;

            // ---- stage targets, smoothed once per chunk (independent of the host block size)
            for (int s = 0; s < kStages; ++s)
            {
                const float tgt = p.on[s] ? juce::jlimit(0.0f, 1.0f, p.amt[s] / 100.0f) : 0.0f;
                const float ta_ = p.a[s] ? 1.0f : 0.0f, tb_ = p.b[s] ? 1.0f : 0.0f;
                if (!primed) { e[s] = tgt; ta[s] = ta_; tb[s] = tb_; }
                else { e[s] += (tgt - e[s]) * 0.35f; ta[s] += (ta_ - ta[s]) * 0.35f; tb[s] += (tb_ - tb[s]) * 0.35f; }
            }
            primed = true;
            const int mClean = juce::jlimit(0, 2, p.mode[Clean]), mFix = juce::jlimit(0, 2, p.mode[Fix]), mTone = juce::jlimit(0, 2, p.mode[Tone]),
                      mCtl = juce::jlimit(0, 2, p.mode[Control]), mCol = juce::jlimit(0, 2, p.mode[Color]), mVoi = juce::jlimit(0, 2, p.mode[Voices]),
                      mSpc = juce::jlimit(0, 2, p.mode[Reverb]), mFin = juce::jlimit(0, 2, p.mode[Finish]);
            const float cl = e[Clean], fx = e[Fix], tn = e[Tone], ct = e[Control], co = e[Color], vo = e[Voices], sp = e[Reverb], dl = e[Delay], fn = e[Finish];

            // CLEAN
            const float autoLc = 20.0f + ta[Clean] * cl * (45.0f + 20.0f * (float) mClean + 35.0f * cl);
            const float lcF = std::max(p.lowcutHz, autoLc), hcF = p.highcutHz;
            const bool lcOn = lcF > 21.0f, hcOn = hcF < 19500.0f;
            const float dnDepth = tb[Clean] * cl * (12.0f + 6.0f * (float) mClean);
            const float dnThr = -68.0f + 22.0f * cl + 3.0f * (float) mClean;
            // FIX
            static const float kFixScale[3] = { 0.6f, 1.0f, 1.4f };
            const float rsA = fx * ta[Fix] * kFixScale[mFix], rsB = fx * tb[Fix] * kFixScale[mFix];   // A: boxy + nasal bands, B: harsh band
            // CONTROL
            static const float kRatio[3] = { 2.2f, 3.5f, 6.0f }, kAtk[3] = { 0.015f, 0.024f, 0.006f }, kRel[3] = { 0.22f, 0.13f, 0.09f };
            const float ratio = juce::jlimit(1.2f, 20.0f, kRatio[mCtl] * std::pow(2.0f, (p.ctlRatio - 50.0f) / 50.0f));
            const float atk = tc(kAtk[mCtl] * std::pow(4.0f, (p.ctlAtk - 50.0f) / 50.0f)), rel = tc(kRel[mCtl] * std::pow(4.0f, (p.ctlRel - 50.0f) / 50.0f));
            const float thr = -9.0f - 22.0f * ct, knee = 8.0f;
            const float makeup = toGain(ta[Control] * 0.6f * -thr * (1.0f - 1.0f / ratio) * std::min(1.0f, ct * 8.0f));
            // DE-ESSER: band-pass detector + subtractive cut around the chosen frequency
            const float de = e[DeEss];
            static const float kDeQ[3] = { 2.5f, 1.4f, 0.8f };
            const bool deListen = ta[DeEss] > 0.5f && de > 0.002f;
            const float deRelC = tc(tb[DeEss] > 0.5f ? 0.06f : 0.02f);
            const float deOffset = 18.0f - 12.0f * de, deMaxCut = p.deMax * std::min(1.0f, de * 4.0f);
            updateDeEss(p.deFreq, kDeQ[juce::jlimit(0, 2, p.mode[DeEss])]);
            // COLOR
            const float driveTape = 1.0f + 3.0f * co, driveTube = 1.0f + 2.5f * co, tubeBias = 0.12f * co;
            const float exW = ((mCol == 2 ? 0.8f : 0.0f) + 0.35f * ta[Color]) * co, thumpW = tb[Color] * co * 0.25f;
            // SPACE
            static const float kRoom[3] = { 0.5f, 0.25f, 0.85f }, kDamp[3] = { 0.3f, 0.55f, 0.55f }, kPre[3] = { 0.016f, 0.006f, 0.032f };
            const int preSamp = std::min(preLen - 2, (int) (fs * kPre[mSpc]));
            const float rvGain = 0.5f * sp, dlGain = 0.75f * dl, duckR = tb[Reverb], duckD = tb[Delay];
            const bool pingPong = ta[Delay] > 0.5f;
            const float fb = 0.38f;
            // VOICES
            const bool vLow = ta[Voices] > 0.5f, vSoft = tb[Voices] > 0.5f;
            const float vInc1 = 2.0f * juce::MathConstants<float>::pi * 0.35f / (float) fs, vInc2 = 2.0f * juce::MathConstants<float>::pi * 0.27f / (float) fs;
            // HARMONY
            const float hm = e[Harmony];
            const int mHar = juce::jlimit(0, 2, p.mode[Harmony]);
            int nAct = 0; bool hAct[6];
            for (int v = 0; v < 6; ++v) { hAct[v] = hm > 0.002f && p.hv[v]; nAct += hAct[v] ? 1 : 0; }
            const float hGain = hm * 2.6f / std::sqrt((float) std::max(1, nAct));   // at 100 % each voice is about 5 dB louder than the lead
            leadG += (((!p.hLead && nAct > 0) ? 0.0f : 1.0f) - leadG) * 0.35f;       // lead muted only while at least one interval voice is on
            const bool hSpread = ta[Harmony] > 0.5f, hSoft = tb[Harmony] > 0.5f;
            float hL[6], hR[6], hRatio[6];
            if (trkH.voiced && trkH.period > 1.0f)
            {   // the two crossfaded grains are half a grain apart: make that a whole number of pitch periods so they add in phase
                int k = 2 * std::max(1, (int) std::lround(0.03f * (float) fs / (2.0f * trkH.period)));
                while (k > 2 && (float) k * trkH.period > 0.045f * (float) fs) k -= 2;
                hLen += ((float) k * trkH.period - hLen) * 0.1f;
            }
            for (int v = 0; v < 6; ++v)
            {
                static const float kPan[6] = { 0.0f, -0.55f, 0.55f, -0.85f, 0.85f, 0.0f };
                const float ang = ((hSpread ? kPan[v] : 0.0f) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                hL[v] = std::cos(ang); hR[v] = std::sin(ang);
                const float target = harmonyShift(v, mHar, p.hkey);
                if (hAct[v] && !hPrev[v]) hSemi[v] = target; else hSemi[v] += (target - hSemi[v]) * 0.3f;
                hPrev[v] = hAct[v];
                hRatio[v] = std::pow(2.0f, hSemi[v] / 12.0f);
            }
            const float vsp = juce::jlimit(0.0f, 1.0f, p.vSpread / 100.0f), vq = juce::MathConstants<float>::pi * 0.25f;
            const float vgL0 = std::cos((1.0f - vsp) * vq), vgR0 = std::sin((1.0f - vsp) * vq), vgL1 = std::cos((1.0f + vsp) * vq), vgR1 = std::sin((1.0f + vsp) * vq);
            const float vms = (float) fs * 0.001f, vGain = (mVoi == 0 ? 0.6f : mVoi == 1 ? 0.55f : 0.7f) * vo;
            // FINISH
            static const float kDrive[3] = { 4.0f, 8.0f, 12.0f }, kLimRel[3] = { 0.15f, 0.1f, 0.07f };
            const bool limActive = fn * ta[Finish] > 0.002f;
            const float preGain = toGain(fn * ta[Finish] * kDrive[mFin]), clipK = tb[Finish] * (fn > 0.002f ? 1.0f : 0.0f);
            const float ceil = juce::Decibels::decibelsToGain(-1.0f), aLimRel = tc(kLimRel[mFin]);

            updateFilters(mTone, lcF, hcF, tn, ta[Tone], tb[Tone], ta[Reverb]);

            // EQ
            const float eqA = e[Eq];
            float eqGs[4]; bool eqActive = false;
            for (int b = 0; b < 4; ++b) { eqGs[b] = p.eqG[b] * eqA * ((b == 0 || b == 3) ? ta[Eq] : tb[Eq]); if (std::abs(eqGs[b]) > 0.01f) eqActive = true; }
            updateEq(p, eqGs);

            float fxRed = 0.0f, grMax = 0.0f, deRedMax = 0.0f;
            for (int i = 0; i < c; ++i)
            {
                float x[2] = { L[i], R[i] };
                dry[0][i] = x[0]; dry[1][i] = x[1];
                const float peakIn = std::max(std::abs(x[0]), std::abs(x[1]));
                tapPk[0][i] = peakIn;

                // ---------------- CLEAN
                if (lcOn) for (int ch = 0; ch < 2; ++ch) x[ch] = lcB.process(ch, lowcut.process(ch, x[ch]));
                if (hcOn) for (int ch = 0; ch < 2; ++ch) x[ch] = hcB.process(ch, hcA.process(ch, x[ch]));
                const float lvl = std::max(std::abs(x[0]), std::abs(x[1]));
                envFull += (lvl - envFull) * aEnv;
                const float fullDb = toDb(envFull);
                if (dnDepth > 0.01f)
                {
                    const float red = fullDb < dnThr ? std::min(dnDepth, (dnThr - fullDb) * 0.7f) : 0.0f;
                    const float tg = toGain(-red);
                    gNoise += (tg - gNoise) * (tg > gNoise ? aNoiseOpen : aNoiseClose);
                    x[0] *= gNoise; x[1] *= gNoise;
                }
                else gNoise = 1.0f;
                tapPk[1][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                anBuf[0][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // analyzer point 0: before FIX

                // ---------------- FIX: three self-calibrating resonance cuts (boxy, nasal, harsh)
                if (rsA > 0.002f || rsB > 0.002f)
                {
                    float bp[3][2];
                    for (int b = 0; b < 3; ++b) for (int ch = 0; ch < 2; ++ch) bp[b][ch] = band[b].process(ch, x[ch]);
                    for (int b = 0; b < 3; ++b)
                    {
                        const float pk = std::max(std::abs(bp[b][0]), std::abs(bp[b][1]));
                        envB[b] += (pk - envB[b]) * (pk > envB[b] ? aBAtk : aBRel);
                        const float relDb = 20.0f * std::log10((envB[b] + 1e-9f) / (envFull + 1e-9f));
                        const float rsb = b < 2 ? rsA : rsB;
                        float red = 0.0f;
                        if (fullDb > -50.0f)
                        {
                            avgRel[b] += (relDb - avgRel[b]) * aAvg;
                            red = std::min(6.0f * rsb, std::max(0.0f, relDb - (avgRel[b] + 2.0f)) * 0.8f);
                        }
                        fxRed = std::max(fxRed, red);
                        gBand[b] += (toGain(-red) - gBand[b]) * 0.02f;
                        for (int ch = 0; ch < 2; ++ch) x[ch] -= bp[b][ch] * (1.0f - gBand[b]);
                    }
                }
                else for (int b = 0; b < 3; ++b) gBand[b] = 1.0f;
                tapPk[2][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                anBuf[1][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 1: after FIX / before EQ

                // ---------------- EQ: low shelf, two bells, high shelf (drawn and dragged in the editor)
                if (eqActive)
                    for (int ch = 0; ch < 2; ++ch) { float y = x[ch]; for (int b = 0; b < 4; ++b) y = eqBq[b].process(ch, y); x[ch] = y; }
                tapPk[3][i] = std::max(std::abs(x[0]), std::abs(x[1]));
                anBuf[2][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 2: after EQ / before TONE

                // ---------------- TONE
                if (tn > 0.002f)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float y = warm.process(ch, x[ch]);
                        y = clarCut.process(ch, y);
                        y = clarBoost.process(ch, y);
                        y = pres.process(ch, y);
                        x[ch] = airShelf.process(ch, y);
                    }
                tapPk[4][i] = std::max(std::abs(x[0]), std::abs(x[1]));
                anBuf[3][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 3: after TONE

                // ---------------- CONTROL: level rider (slow) then linked compressor
                if (ct > 0.002f)
                {
                    if (tb[Control] > 0.002f)
                    {
                        rmsEnv += (0.5f * (x[0] * x[0] + x[1] * x[1]) - rmsEnv) * aRms;
                        if ((i & 15) == 0)
                        {
                            const float db = 10.0f * std::log10(rmsEnv + 1e-12f);
                            const float want = db > -55.0f ? juce::jlimit(-6.0f, 9.0f, -19.0f - db) : 0.0f;
                            riderDb += (want * ct * tb[Control] - riderDb) * aRider16;
                            riderLin = toGain(riderDb);
                        }
                        x[0] *= riderLin; x[1] *= riderLin;
                    }
                    else { riderDb = 0.0f; riderLin = 1.0f; }
                    const float pk = std::max(std::abs(x[0]), std::abs(x[1]));
                    const float inDb = toDb(pk);
                    const float over = inDb - thr;
                    float outDb = inDb;
                    if (2.0f * over > knee) outDb = thr + over / ratio;
                    else if (2.0f * over >= -knee) { const float q = over + knee * 0.5f; outDb = inDb + (1.0f / ratio - 1.0f) * q * q / (2.0f * knee); }
                    const float want = inDb - outDb;
                    grDb += (want - grDb) * (want > grDb ? atk : rel);
                    grMax = std::max(grMax, grDb);
                    const float g = toGain(-grDb) * makeup;
                    x[0] *= g; x[1] *= g;
                }
                else { grDb = 0.0f; riderDb = 0.0f; riderLin = 1.0f; }
                tapPk[5][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                // ---------------- DE-ESSER (after the compressor, which tends to bring the sibilance up)
                anBuf[4][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 4: before DE-ESSER
                if (de > 0.002f)
                {
                    const float bpd[2] = { deBp.process(0, x[0]), deBp.process(1, x[1]) };
                    { const float lvD = std::max(std::abs(x[0]), std::abs(x[1])); envFullD += (lvD - envFullD) * (lvD > envFullD ? aSibAtk : aSibRel); }
                    const float pkB = std::max(std::abs(bpd[0]), std::abs(bpd[1]));
                    envDe += (pkB - envDe) * (pkB > envDe ? aSibAtk : aSibRel);
                    float red = 0.0f;
                    if (toDb(envFullD) > -55.0f)
                    {
                        const float relD = 20.0f * std::log10((envDe + 1e-9f) / (envFullD + 1e-9f));
                        red = std::min(deMaxCut, std::max(0.0f, relD + deOffset) * 1.6f);
                    }
                    deRedMax = std::max(deRedMax, red);
                    const float tgD = toGain(-red);
                    gDe += (tgD - gDe) * (tgD < gDe ? aSibAtk : deRelC);
                    if (deListen) { x[0] = bpd[0]; x[1] = bpd[1]; }   // LISTEN: hear the band that is being controlled
                    else { x[0] -= bpd[0] * (1.0f - gDe); x[1] -= bpd[1] * (1.0f - gDe); }
                }
                else gDe = 1.0f;
                tapPk[6][i] = std::max(std::abs(x[0]), std::abs(x[1]));
                anBuf[5][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 5: after DE-ESSER

                // ---------------- COLOR: tape / tube / exciter + optional sparkle and low thump (added terms are DC-blocked)
                if (co > 0.002f)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float v = x[ch];
                        float term = 0.0f;
                        if (mCol == 0) term += 0.8f * co * (std::tanh(driveTape * v) / driveTape - v);
                        else if (mCol == 1) term += co * ((std::tanh(driveTube * (v + tubeBias)) - std::tanh(driveTube * tubeBias)) / driveTube - v);
                        const float hf = exHp.process(ch, v), lo = thumpLp.process(ch, v);
                        term += exW * std::tanh(1.5f * hf) + thumpW * std::tanh(2.0f * lo);
                        const float dc = term - dcX[ch] + 0.995f * dcY[ch];
                        dcX[ch] = term; dcY[ch] = dc;
                        x[ch] = v + dc;
                    }
                tapPk[7][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                // ---------------- VOICES: doubled / stacked / wide copies of the centre, low end kept mono, optional softening
                const float midIn = 0.5f * (x[0] + x[1]);
                vBuf[(size_t) vw] = midIn;
                if (vo > 0.002f)
                {
                    lfo1 += vInc1; if (lfo1 > 6.2831853f) lfo1 -= 6.2831853f;
                    lfo2 += vInc2; if (lfo2 > 6.2831853f) lfo2 -= 6.2831853f;
                    const float s1 = std::sin(lfo1), s2 = std::sin(lfo2);
                    auto tap = [&](float d) {
                        float rp = (float) vw - d; while (rp < 0.0f) rp += (float) vLen;
                        const int i0 = (int) rp, i1 = (i0 + 1) % vLen; const float fr = rp - (float) i0;
                        return vBuf[(size_t) i0] * (1.0f - fr) + vBuf[(size_t) i1] * fr;
                    };
                    float wv[2];
                    if (mVoi == 0)      { wv[0] = tap(vms * (18.0f + 1.6f * s1)); wv[1] = tap(vms * (26.0f + 1.8f * s2)); }
                    else if (mVoi == 1) { wv[0] = 0.6f * tap(vms * (16.0f + 1.5f * s1)) + 0.5f * tap(vms * (33.0f + 2.0f * s2));
                                          wv[1] = 0.6f * tap(vms * (22.0f + 1.7f * s2)) + 0.5f * tap(vms * (41.0f + 2.2f * s1)); }
                    else                { wv[0] = tap(vms * (7.0f + 0.4f * s1)); wv[1] = -tap(vms * (11.0f + 0.5f * s2)); }
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        if (vLow) wv[ch] = vHp.process(ch, wv[ch]);
                        if (vSoft) wv[ch] = vLp.process(ch, wv[ch]);
                    }
                    x[0] += vGain * (wv[0] * vgL0 + wv[1] * vgL1);   // wv[0] = left copy, wv[1] = right copy, panned out to +-spread
                    x[1] += vGain * (wv[0] * vgR0 + wv[1] * vgR1);
                }
                tapPk[8][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                // ---------------- HARMONY: lead pitch tracker + up to six pitch-shifted copies (two crossfaded grains on the voice delay line)
                if (hm > 0.002f)
                {
                    trkH.push(midIn);
                    float wl = 0.0f, wr = 0.0f;
                    for (int v = 0; v < 6; ++v)
                        if (hAct[v])
                        {
                            const float ph = hPhase[v], ph2 = ph >= 0.5f ? ph - 0.5f : ph + 0.5f;
                            const float g1 = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * ph);
                            auto tap = [&](float d) {
                                float rp = (float) vw - d; while (rp < 0.0f) rp += (float) vLen;
                                const int i0 = (int) rp, i1 = (i0 + 1) % vLen; const float fr = rp - (float) i0;
                                return vBuf[(size_t) i0] * (1.0f - fr) + vBuf[(size_t) i1] * fr;
                            };
                            const float y = tap(ph * hLen) * g1 + tap(ph2 * hLen) * (1.0f - g1);
                            float np = ph + (1.0f - hRatio[v]) / hLen;
                            np -= std::floor(np);
                            hPhase[v] = np;
                            wl += y * hL[v]; wr += y * hR[v];
                        }
                    if (hSoft) { wl = hLp.process(0, wl); wr = hLp.process(1, wr); }
                    x[0] = x[0] * leadG + hGain * wl; x[1] = x[1] * leadG + hGain * wr;
                }
                else { x[0] *= leadG; x[1] *= leadG; }
                if (++vw >= vLen) vw = 0;
                tapPk[9][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                proc[0][i] = x[0]; proc[1][i] = x[1];

                // ---------------- SPACE send (filtered mono) + ducking envelope from the dry input
                const float sendL = sendLp.process(0, sendHp.process(0, x[0]));
                const float sendR = sendLp.process(1, sendHp.process(1, x[1]));
                send[0][i] = sendL; send[1][i] = sendR;
                preBuf[(size_t) pw] = 0.5f * (sendL + sendR);
                int pr0 = pw - preSamp; if (pr0 < 0) pr0 += preLen;
                revIn[i] = preBuf[(size_t) pr0];
                if (++pw >= preLen) pw = 0;
                duck += (peakIn - duck) * (peakIn > duck ? duckAtk : duckRel);
                duckBuf[i] = duck;
            }

            anPos = (anPos + c) & kAnMask; anPub.store(anPos, std::memory_order_release);

            // reverb on the send
            if (sp > 0.002f)
            {
                juce::Reverb::Parameters rp;
                rp.roomSize = kRoom[mSpc]; rp.damping = kDamp[mSpc]; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
                reverb.setParameters(rp);
                std::copy(revIn, revIn + c, revOut[0]); std::copy(revIn, revIn + c, revOut[1]);
                reverb.processStereo(revOut[0], revOut[1], c);
            }
            else { std::fill(revOut[0], revOut[0] + c, 0.0f); std::fill(revOut[1], revOut[1] + c, 0.0f); }

            float limMin = 1.0f;
            for (int i = 0; i < c; ++i)
            {
                delaySmoothed += (delayTarget - delaySmoothed) * 0.0004f;
                float dOut[2] = { 0, 0 };
                const bool dlOn = dlGain > 0.002f;
                if (dlOn || std::abs(dlyLp[0]) + std::abs(dlyLp[1]) > 1e-6f)
                {
                    float rp = (float) dw - delaySmoothed;
                    while (rp < 0.0f) rp += (float) delayLen;
                    const int i0 = (int) rp; const float fr = rp - (float) i0; const int i1 = (i0 + 1) % delayLen;
                    float rd[2];
                    for (int ch = 0; ch < 2; ++ch) rd[ch] = delayBuf[ch][(size_t) i0] * (1.0f - fr) + delayBuf[ch][(size_t) i1] * fr;
                    dlyLp[0] += (rd[0] - dlyLp[0]) * 0.45f; dlyLp[1] += (rd[1] - dlyLp[1]) * 0.45f;
                    const float mono = 0.5f * (send[0][i] + send[1][i]) * (dlOn ? 1.0f : 0.0f);
                    delayBuf[0][(size_t) dw] = mono + fb * (pingPong ? dlyLp[1] : dlyLp[0]);
                    delayBuf[1][(size_t) dw] = (pingPong ? 0.0f : mono) + fb * (pingPong ? dlyLp[0] : dlyLp[1]);
                    dOut[0] = dlyLp[0]; dOut[1] = dlyLp[1];
                }
                if (++dw >= delayLen) dw = 0;

                const float d = juce::jlimit(0.0f, 1.0f, duckBuf[i] * 7.0f);
                const float gR = 1.0f - 0.45f * d * duckR, gD = 1.0f - 0.85f * d * duckD;
                float y[2];
                for (int ch = 0; ch < 2; ++ch) y[ch] = proc[ch][i] + revOut[ch][i] * rvGain * gR;
                tapPk[10][i] = std::max(std::abs(y[0]), std::abs(y[1]));
                for (int ch = 0; ch < 2; ++ch) y[ch] += dOut[ch] * dlGain * gD;
                tapPk[11][i] = std::max(std::abs(y[0]), std::abs(y[1]));

                // ---------------- FINISH: drive into a soft clipper and a peak limiter at -1 dB
                if (limActive)
                {
                    y[0] *= preGain; y[1] *= preGain;
                    if (clipK > 0.002f)
                        for (int ch = 0; ch < 2; ++ch)
                        {
                            const float ax = std::abs(y[ch]);
                            const float cl2 = ax < 0.7f ? y[ch] : std::copysign(0.7f + 0.3f * std::tanh((ax - 0.7f) / 0.3f), y[ch]);
                            y[ch] += clipK * (cl2 - y[ch]);
                        }
                    const float pk = std::max(std::abs(y[0]), std::abs(y[1]));
                    const float tgt = pk > ceil ? ceil / pk : 1.0f;
                    gLim += (tgt - gLim) * (tgt < gLim ? 0.6f : aLimRel);
                    y[0] *= gLim; y[1] *= gLim;
                    limMin = std::min(limMin, gLim);
                }
                else
                {
                    gLim += (1.0f - gLim) * aLimRel;
                    if (clipK > 0.002f)
                        for (int ch = 0; ch < 2; ++ch)
                        {
                            const float ax = std::abs(y[ch]);
                            const float cl2 = ax < 0.7f ? y[ch] : std::copysign(0.7f + 0.3f * std::tanh((ax - 0.7f) / 0.3f), y[ch]);
                            y[ch] += clipK * (cl2 - y[ch]);
                        }
                }
                float o[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    float v = (dry[ch][i] * (1.0f - mixAmt) + y[ch] * mixAmt) * outGain;
                    v = softClip(v);
                    o[ch] = std::isfinite(v) ? v : 0.0f;
                }
                finPk[i] = std::max(std::abs(y[0]), std::abs(y[1]));
                L[i] = o[0]; R[i] = o[1];
            }

            // ---- history columns (20 ms): peak of every tap
            for (int i = 0; i < c; ++i)
            {
                for (int t = 0; t < 12; ++t) colPk[t] = std::max(colPk[t], tapPk[t][i]);
                colPk[12] = std::max(colPk[12], finPk[i]);
                if (++colCount >= colLen)
                {
                    const int pos = histPos.load(std::memory_order_relaxed);
                    for (int t = 0; t < kTaps; ++t) { hist[t][(size_t) pos].store(colPk[t], std::memory_order_relaxed); colPk[t] = 0.0f; }
                    histPos.store((pos + 1) % kHist, std::memory_order_release);
                    colCount = 0;
                }
            }

            leadOut.store(trkH.voiced ? trkH.midi : -1.0f, std::memory_order_relaxed);
            infoMax[Clean] = std::max(infoMax[Clean], -toDb(gNoise));
            infoMax[Fix] = std::max(infoMax[Fix], fxRed);
            infoMax[Control] = std::max(infoMax[Control], grMax);
            infoMax[DeEss] = std::max(infoMax[DeEss], deRedMax);
            infoMax[Finish] = std::max(infoMax[Finish], -toDb(limMin));
            done += c;
        }
        for (int s = 0; s < kStages; ++s) info[s].store(infoMax[s], std::memory_order_relaxed);
    }

    float histAt(int tap, int idx) const { return hist[tap][(size_t) idx].load(std::memory_order_relaxed); }
    int histWritePos() const { return histPos.load(std::memory_order_acquire); }
    // last n samples (mono) at analyzer point pt: 0 = before FIX, 1 = after FIX, 2 = after TONE (read by the editor for its spectrum)
    void copyAnalyzer(int pt, float* out, int n) const
    {
        const int end = anPub.load(std::memory_order_acquire);
        for (int k = 0; k < n; ++k) out[k] = anBuf[pt][(size_t) ((end - n + k) & kAnMask)];
    }
    float leadMidiNow() const { return leadOut.load(std::memory_order_relaxed); }   // detected note of the lead (MIDI), -1 = none
    float infoAt(int s) const { return info[s].load(std::memory_order_relaxed); }

private:
    // Semitone shift of harmony voice v (0..5 = -8ve, -3rd, +3rd, +5th, +6th, +8ve). Scale modes follow the detected lead note inside the key.
    float harmonyShift(int v, int mode, int key) const
    {
        static const int kFixed[6] = { -12, -4, 4, 7, 9, 12 }, kDeg[6] = { 0, -2, 2, 4, 5, 0 };
        if (v == 0) return -12.0f;
        if (v == 5) return 12.0f;
        if (mode == 2) return (float) kFixed[v];
        static const int kMaj[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMin[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int* sc = mode == 0 ? kMaj : kMin;
        const int rel = (int) std::lround(trkH.midi) - key;
        const int pc = ((rel % 12) + 12) % 12;
        int best = 0, bestDist = 99;
        for (int d = 0; d < 7; ++d) { const int dist = std::min(std::abs(sc[d] - pc), 12 - std::abs(sc[d] - pc)); if (dist < bestDist) { bestDist = dist; best = d; } }
        const int td = best + kDeg[v];
        const int oct = (int) std::floor((double) td / 7.0), dd = ((td % 7) + 7) % 7;
        return (float) (12 * oct + sc[dd] - sc[best]);
    }

    void updateDeEss(float f, float q)
    {
        if (std::abs(f - deKey[0]) < 1.0f && std::abs(q - deKey[1]) < 0.001f) return;
        deKey = { f, q };
        deBp.bandPass(fs, f, q);
    }

    void updateEq(const Params& p, const float* gains)
    {
        const std::array<float, 10> key { p.eqF[0], p.eqF[1], p.eqF[2], p.eqF[3], gains[0], gains[1], gains[2], gains[3], p.eqQ[1], p.eqQ[2] };
        bool changed = false;
        for (size_t i = 0; i < key.size(); ++i) if (std::abs(key[i] - eqKey[i]) > 0.0005f) changed = true;
        if (!changed) return;
        eqKey = key;
        eqBq[0].lowShelf(fs, key[0], key[4]);
        eqBq[1].peak(fs, key[1], key[8], key[5]);
        eqBq[2].peak(fs, key[2], key[9], key[6]);
        eqBq[3].highShelf(fs, key[3], key[7]);
    }

    void updateFilters(int mTone, float lcF, float hcF, float tn, float taAir, float tbBody, float taBright)
    {
        static const float W[3][4] = { { 1.0f, 0.5f, 0.4f, 0.5f }, { 0.6f, 0.8f, 0.8f, 0.7f }, { 0.25f, 0.9f, 1.0f, 1.0f } };
        const std::array<float, 8> key { lcF, 6.0f * tn * W[mTone][0] * tbBody, -4.0f * tn * W[mTone][1], 2.4f * tn * W[mTone][1],
                                         5.5f * tn * W[mTone][2], 8.0f * tn * W[mTone][3] * taAir, hcF, 5000.0f + 6000.0f * taBright };
        bool changed = false;
        for (size_t i = 0; i < key.size(); ++i) if (std::abs(key[i] - lastKey[i]) > (i == 0 || i == 6 || i == 7 ? 0.5f : 0.0005f)) changed = true;
        if (!changed) return;
        lastKey = key;
        lowcut.highPass(fs, key[0], 0.5412); lcB.highPass(fs, key[0], 1.3066);     // 24 dB/oct
        hcA.lowPass(fs, key[6], 0.5412);     hcB.lowPass(fs, key[6], 1.3066);
        warm.lowShelf(fs, 170.0, key[1]);
        clarCut.peak(fs, 340.0, 1.0, key[2]);
        clarBoost.peak(fs, 2600.0, 0.8, key[3]);
        pres.peak(fs, 4800.0, 0.9, key[4]);
        airShelf.highShelf(fs, 11000.0, key[5]);
        sendLp.lowPass(fs, key[7], 0.707);
    }

    double fs = 44100.0;
    bool primed = false;
    Biquad deBp;
    std::array<float, 2> deKey { -1.0f, -1.0f };
    float envFullD = 0, envDe = 0, gDe = 1.0f;
    Biquad lowcut, lcB, hcA, hcB, vHp, vLp, sibHp, band[3], warm, clarCut, clarBoost, pres, airShelf, exHp, thumpLp, sendHp, sendLp;
    std::array<float, 8> lastKey {};
    float e[kStages] = {}, ta[kStages] = {}, tb[kStages] = {};
    float envFull = 0, envSib = 0, gNoise = 1, gSib = 1, grDb = 0, duck = 0;
    float envB[3] = {}, avgRel[3] = { -18.0f, -18.0f, -18.0f }, gBand[3] = { 1, 1, 1 };
    float riderDb = 0, riderLin = 1, rmsEnv = 0, gLim = 1, dcX[2] = {}, dcY[2] = {};
    std::vector<float> delayBuf[2], preBuf;
    int delayLen = 1, dw = 0, preLen = 1, pw = 0, vLen = 1, vw = 0;
    std::vector<float> vBuf;
    float lfo1 = 0, lfo2 = 1.3f;
    Biquad hLp;
    float hPhase[6] = {}, hSemi[6] = {}, hLen = 1440.0f;
    bool hPrev[6] = {};
    PitchTracker trkH;
    float leadG = 1.0f;
    Biquad eqBq[4];
    std::array<float, 10> eqKey {};
    std::atomic<float> leadOut { -1.0f };
    float dlyLp[2] = { 0, 0 }, delaySmoothed = -1.0f;
    juce::Reverb reverb;
    float dry[2][kChunk], proc[2][kChunk], send[2][kChunk], revIn[kChunk], revOut[2][kChunk], duckBuf[kChunk], tapPk[12][kChunk], finPk[kChunk];
    int colLen = 960, colCount = 0;
    float colPk[kTaps] = {};
    std::atomic<float> hist[kTaps][kHist];
    std::atomic<int> histPos { 0 };
    static constexpr int kAnMask = 4095;
    float anBuf[6][kAnMask + 1] = {};
    int anPos = 0;
    std::atomic<int> anPub { 0 };
    std::atomic<float> info[kStages];
};
} // namespace nfvs
