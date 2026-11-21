#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include "Stages.h"

// NF Vocal Suite signal chain: CLEAN -> FIX -> TONE -> CONTROL -> COLOR -> SPACE -> FINISH.
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

class Chain
{
public:
    static constexpr int kChunk = 512, kHist = 256, kTaps = 10;   // tap 0 = input, tap k = after stage k

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
        vHp.highPass(fs, 250.0, 0.707); vLp.lowPass(fs, 5000.0, 0.707);
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
        for (auto* b : { &lowcut, &lcB, &hcA, &hcB, &vHp, &vLp, &sibHp, &band[0], &band[1], &band[2], &warm, &clarCut, &clarBoost, &pres, &airShelf, &exHp, &thumpLp, &sendHp, &sendLp })
            b->reset();
        reverb.reset();
        std::fill(delayBuf[0].begin(), delayBuf[0].end(), 0.0f);
        std::fill(delayBuf[1].begin(), delayBuf[1].end(), 0.0f);
        std::fill(preBuf.begin(), preBuf.end(), 0.0f);
        dw = 0; pw = 0; envFull = envSib = 0; gNoise = gSib = 1.0f; grDb = 0; duck = 0; dlyLp[0] = dlyLp[1] = 0;
        delaySmoothed = -1.0f; riderDb = 0; riderLin = 1.0f; rmsEnv = 0; gLim = 1.0f;
        for (int b = 0; b < 3; ++b) { envB[b] = 0; avgRel[b] = -18.0f; gBand[b] = 1.0f; }
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
        std::fill(vBuf.begin(), vBuf.end(), 0.0f); vw = 0; lfo1 = 0; lfo2 = 1.3f;
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
            const int mClean = juce::jlimit(0, 2, p.mode[0]), mFix = juce::jlimit(0, 2, p.mode[1]), mTone = juce::jlimit(0, 2, p.mode[2]),
                      mCtl = juce::jlimit(0, 2, p.mode[3]), mCol = juce::jlimit(0, 2, p.mode[4]), mVoi = juce::jlimit(0, 2, p.mode[Voices]),
                      mSpc = juce::jlimit(0, 2, p.mode[Reverb]), mFin = juce::jlimit(0, 2, p.mode[Finish]);
            const float cl = e[0], fx = e[1], tn = e[2], ct = e[3], co = e[4], vo = e[Voices], sp = e[Reverb], dl = e[Delay], fn = e[Finish];

            // CLEAN
            const float autoLc = 20.0f + ta[0] * cl * (45.0f + 20.0f * (float) mClean + 35.0f * cl);
            const float lcF = std::max(p.lowcutHz, autoLc), hcF = p.highcutHz;
            const bool lcOn = lcF > 21.0f, hcOn = hcF < 19500.0f;
            const float dnDepth = tb[0] * cl * (12.0f + 6.0f * (float) mClean);
            const float dnThr = -68.0f + 22.0f * cl + 3.0f * (float) mClean;
            // FIX
            static const float kFixScale[3] = { 0.6f, 1.0f, 1.4f };
            const float ds = fx * ta[1] * kFixScale[mFix], rs = fx * tb[1] * kFixScale[mFix];
            // CONTROL
            static const float kRatio[3] = { 2.2f, 3.5f, 6.0f }, kAtk[3] = { 0.015f, 0.024f, 0.006f }, kRel[3] = { 0.22f, 0.13f, 0.09f };
            const float ratio = kRatio[mCtl], atk = tc(kAtk[mCtl]), rel = tc(kRel[mCtl]);
            const float thr = -9.0f - 22.0f * ct, knee = 8.0f;
            const float makeup = toGain(ta[3] * 0.6f * -thr * (1.0f - 1.0f / ratio) * std::min(1.0f, ct * 8.0f));
            // COLOR
            const float driveTape = 1.0f + 3.0f * co, driveTube = 1.0f + 2.5f * co, tubeBias = 0.12f * co;
            const float exW = ((mCol == 2 ? 0.8f : 0.0f) + 0.35f * ta[4]) * co, thumpW = tb[4] * co * 0.25f;
            // SPACE
            static const float kRoom[3] = { 0.5f, 0.25f, 0.85f }, kDamp[3] = { 0.3f, 0.55f, 0.55f }, kPre[3] = { 0.016f, 0.006f, 0.032f };
            const int preSamp = std::min(preLen - 2, (int) (fs * kPre[mSpc]));
            const float rvGain = 0.5f * sp, dlGain = 0.75f * dl, duckR = tb[Reverb], duckD = tb[Delay];
            const bool pingPong = ta[Delay] > 0.5f;
            const float fb = 0.38f;
            // VOICES
            const bool vLow = ta[Voices] > 0.5f, vSoft = tb[Voices] > 0.5f;
            const float vInc1 = 2.0f * juce::MathConstants<float>::pi * 0.35f / (float) fs, vInc2 = 2.0f * juce::MathConstants<float>::pi * 0.27f / (float) fs;
            const float vms = (float) fs * 0.001f, vGain = (mVoi == 0 ? 0.6f : mVoi == 1 ? 0.55f : 0.7f) * vo;
            // FINISH
            static const float kDrive[3] = { 4.0f, 8.0f, 12.0f }, kLimRel[3] = { 0.15f, 0.1f, 0.07f };
            const bool limActive = fn * ta[Finish] > 0.002f;
            const float preGain = toGain(fn * ta[Finish] * kDrive[mFin]), clipK = tb[Finish] * (fn > 0.002f ? 1.0f : 0.0f);
            const float ceil = juce::Decibels::decibelsToGain(-1.0f), aLimRel = tc(kLimRel[mFin]);

            updateFilters(mTone, lcF, hcF, tn, ta[2], tb[2], ta[Reverb]);

            float fxRed = 0.0f, grMax = 0.0f;
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

                // ---------------- FIX: de-esser, then three self-calibrating resonance cuts
                const float hs[2] = { sibHp.process(0, x[0]), sibHp.process(1, x[1]) };
                const float sib = std::max(std::abs(hs[0]), std::abs(hs[1]));
                envSib += (sib - envSib) * (sib > envSib ? aSibAtk : aSibRel);
                if (ds > 0.001f)
                {
                    const float rel2 = 20.0f * std::log10((envSib + 1e-9f) / (envFull + 1e-9f));
                    const float excess = fullDb > -55.0f ? std::max(0.0f, rel2 + 13.0f) : 0.0f;
                    const float red = std::min(11.0f * ds, excess * 1.3f * ds);
                    fxRed = std::max(fxRed, red);
                    gSib += (toGain(-red) - gSib) * 0.02f;
                    for (int ch = 0; ch < 2; ++ch) x[ch] -= hs[ch] * (1.0f - gSib);
                }
                else gSib = 1.0f;
                if (rs > 0.002f)
                {
                    float bp[3][2];
                    for (int b = 0; b < 3; ++b) for (int ch = 0; ch < 2; ++ch) bp[b][ch] = band[b].process(ch, x[ch]);
                    for (int b = 0; b < 3; ++b)
                    {
                        const float pk = std::max(std::abs(bp[b][0]), std::abs(bp[b][1]));
                        envB[b] += (pk - envB[b]) * (pk > envB[b] ? aBAtk : aBRel);
                        const float relDb = 20.0f * std::log10((envB[b] + 1e-9f) / (envFull + 1e-9f));
                        float red = 0.0f;
                        if (fullDb > -50.0f)
                        {
                            avgRel[b] += (relDb - avgRel[b]) * aAvg;
                            red = std::min(6.0f * rs, std::max(0.0f, relDb - (avgRel[b] + 2.0f)) * 0.8f);
                        }
                        fxRed = std::max(fxRed, red);
                        gBand[b] += (toGain(-red) - gBand[b]) * 0.02f;
                        for (int ch = 0; ch < 2; ++ch) x[ch] -= bp[b][ch] * (1.0f - gBand[b]);
                    }
                }
                else for (int b = 0; b < 3; ++b) gBand[b] = 1.0f;
                tapPk[2][i] = std::max(std::abs(x[0]), std::abs(x[1]));
                anBuf[1][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 1: after FIX / before TONE

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
                tapPk[3][i] = std::max(std::abs(x[0]), std::abs(x[1]));
                anBuf[2][(size_t) ((anPos + i) & kAnMask)] = 0.5f * (x[0] + x[1]);   // point 2: after TONE

                // ---------------- CONTROL: level rider (slow) then linked compressor
                if (ct > 0.002f)
                {
                    if (tb[3] > 0.002f)
                    {
                        rmsEnv += (0.5f * (x[0] * x[0] + x[1] * x[1]) - rmsEnv) * aRms;
                        if ((i & 15) == 0)
                        {
                            const float db = 10.0f * std::log10(rmsEnv + 1e-12f);
                            const float want = db > -55.0f ? juce::jlimit(-6.0f, 9.0f, -19.0f - db) : 0.0f;
                            riderDb += (want * ct * tb[3] - riderDb) * aRider16;
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
                tapPk[4][i] = std::max(std::abs(x[0]), std::abs(x[1]));

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
                tapPk[5][i] = std::max(std::abs(x[0]), std::abs(x[1]));

                // ---------------- VOICES: doubled / stacked / wide copies of the centre, low end kept mono, optional softening
                vBuf[(size_t) vw] = 0.5f * (x[0] + x[1]);
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
                        x[ch] += vGain * wv[ch];
                    }
                }
                if (++vw >= vLen) vw = 0;
                tapPk[6][i] = std::max(std::abs(x[0]), std::abs(x[1]));

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
                tapPk[7][i] = std::max(std::abs(y[0]), std::abs(y[1]));
                for (int ch = 0; ch < 2; ++ch) y[ch] += dOut[ch] * dlGain * gD;
                tapPk[8][i] = std::max(std::abs(y[0]), std::abs(y[1]));

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
                for (int t = 0; t < 9; ++t) colPk[t] = std::max(colPk[t], tapPk[t][i]);
                colPk[9] = std::max(colPk[9], finPk[i]);
                if (++colCount >= colLen)
                {
                    const int pos = histPos.load(std::memory_order_relaxed);
                    for (int t = 0; t < kTaps; ++t) { hist[t][(size_t) pos].store(colPk[t], std::memory_order_relaxed); colPk[t] = 0.0f; }
                    histPos.store((pos + 1) % kHist, std::memory_order_release);
                    colCount = 0;
                }
            }

            infoMax[0] = std::max(infoMax[0], -toDb(gNoise));
            infoMax[1] = std::max(infoMax[1], fxRed);
            infoMax[3] = std::max(infoMax[3], grMax);
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
    float infoAt(int s) const { return info[s].load(std::memory_order_relaxed); }

private:
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
    float dlyLp[2] = { 0, 0 }, delaySmoothed = -1.0f;
    juce::Reverb reverb;
    float dry[2][kChunk], proc[2][kChunk], send[2][kChunk], revIn[kChunk], revOut[2][kChunk], duckBuf[kChunk], tapPk[9][kChunk], finPk[kChunk];
    int colLen = 960, colCount = 0;
    float colPk[kTaps] = {};
    std::atomic<float> hist[kTaps][kHist];
    std::atomic<int> histPos { 0 };
    static constexpr int kAnMask = 4095;
    float anBuf[3][kAnMask + 1] = {};
    int anPos = 0;
    std::atomic<int> anPub { 0 };
    std::atomic<float> info[kStages];
};
} // namespace nfvs
