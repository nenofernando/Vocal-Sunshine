#include "PluginEditor.h"
#include "NFVocalSuiteBinaryData.h"
#include "Presets.h"

namespace
{
const juce::Colour kPanel(0xff1b1b1e), kLine(0xff2e2e33), kText(0xfff2efe8), kMuted(0xff8b8f97), kOff(0xff3a3a40), kDark(0xff0c0c0e);
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f, kEnd = juce::MathConstants<float>::pi * 2.75f;
juce::Font font(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Colour stageColour(int s) { return juce::Colour(nfvs::kStageInfo[s].colour); }

void drawSwitch(juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour accent)
{
    g.setColour(on ? accent.withAlpha(0.28f) : juce::Colour(0xff101012)); g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(on ? accent : kOff); g.drawRoundedRectangle(r.reduced(0.5f), r.getHeight() * 0.5f, 1.2f);
    const float d = r.getHeight() - 6.0f;
    g.setColour(on ? accent : juce::Colour(0xff6b6f77));
    g.fillEllipse(on ? r.getRight() - d - 3.0f : r.getX() + 3.0f, r.getY() + 3.0f, d, d);
}
}

void NFVocalSuiteAudioProcessorEditor::KnobLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                                                  float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const bool big = std::min(w, h) > 100;
    const float R = std::min(w, h) * 0.5f - 2.0f;
    const float capR = R * (big ? 0.70f : 0.72f);
    const float a = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0;

    if (big)
    {
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, ang = start + t * (end - start), sx = std::sin(ang), cy = -std::cos(ang);
            const float r1 = R * 0.86f, r2 = R * (i % 5 == 0 ? 1.0f : 0.94f);
            const bool lit = t <= pos + 0.001f && pos > 0.0f;
            g.setColour(lit ? accent : juce::Colour(0xff2a2a30));
            g.drawLine(c.x + sx * r1, c.y + cy * r1, c.x + sx * r2, c.y + cy * r2, i % 5 == 0 ? 3.2f : 2.2f);
        }
        if (pos > 0.0f) // soft glow behind the lit ticks
        {
            g.setColour(accent.withAlpha(0.10f));
            juce::Path arc; arc.addCentredArc(c.x, c.y, R * 0.9f, R * 0.9f, 0.0f, start, a, true);
            g.strokePath(arc, juce::PathStrokeType(10.0f));
        }
    }
    else
    {
        juce::Path track; track.addCentredArc(c.x, c.y, R - 1.5f, R - 1.5f, 0.0f, start, end, true);
        g.setColour(kOff); g.strokePath(track, juce::PathStrokeType(2.5f));
        const float from = bipolar ? start + 0.5f * (end - start) : start;
        juce::Path val; val.addCentredArc(c.x, c.y, R - 1.5f, R - 1.5f, 0.0f, std::min(from, a), std::max(from, a), true);
        g.setColour(juce::Colour(0xffb9bcc4)); g.strokePath(val, juce::PathStrokeType(2.5f));
    }

    g.setColour(juce::Colours::black.withAlpha(0.65f)); g.fillEllipse(c.x - capR, c.y - capR + 3.0f, capR * 2.0f, capR * 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(big ? 0xff34343a : 0xff3a3a40), c.x - capR, c.y - capR, juce::Colour(0xff0e0e10), c.x + capR, c.y + capR, false));
    g.fillEllipse(c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour(big ? 0x33ffffff : 0xff4a4a50)); g.drawEllipse(c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    if (big)
    {
        g.setColour(juce::Colour(0x14ffffff));
        for (float r = capR * 0.2f; r < capR; r += capR * 0.1f) g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.6f);
    }
    const float sx = std::sin(a), cy = -std::cos(a);
    g.setColour(big ? accent : juce::Colour(0xffd9dce2));
    g.drawLine(c.x + sx * capR * 0.5f, c.y + cy * capR * 0.5f, c.x + sx * capR * 0.92f, c.y + cy * capR * 0.92f, big ? 5.0f : 2.5f);
}

NFVocalSuiteAudioProcessorEditor::NFVocalSuiteAudioProcessorEditor(NFVocalSuiteAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    logo = juce::ImageCache::getFromMemory(NFVocalSuiteBinaryData::_10_logo_nf_audio_tools_png,
                                           NFVocalSuiteBinaryData::_10_logo_nf_audio_tools_pngSize);
    auto setup = [this](juce::Slider& s, KnobLook& l, int drag) {
        s.setLookAndFeel(&l);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters(kStart, kEnd, true);
        s.setMouseDragSensitivity(drag);
        s.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        s.setPopupDisplayEnabled(true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible(s);
    };
    setup(knob, look, 240);
    setup(outKnob, smallLook, 200);
    setup(mixKnob, smallLook, 200);
    setup(lowKnob, smallLook, 200);
    setup(highKnob, smallLook, 200);
    lowKnob.textFromValueFunction = [](double v) { return v < 20.5 ? juce::String("Off") : juce::String(juce::roundToInt(v)) + " Hz"; };
    highKnob.textFromValueFunction = [](double v) { return v > 19900.0 ? juce::String("Off") : (v >= 1000.0 ? juce::String(v / 1000.0, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"); };
    lowKnob.setDoubleClickReturnValue(true, 20.0);
    highKnob.setDoubleClickReturnValue(true, 20000.0);
    lowAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "lowcut", lowKnob);
    highAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "highcut", highKnob);
    outKnob.textFromValueFunction = [](double v) { return (v > 0.04 ? "+" : "") + juce::String(v, 1) + " dB"; };
    mixKnob.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " %"; };
    outKnob.setDoubleClickReturnValue(true, 0.0);
    mixKnob.setDoubleClickReturnValue(true, 100.0);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output", outKnob);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "mix", mixKnob);
    knob.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " %"; };
    fftWin.resize(kFftSize); fftData.resize(kFftSize * 2);
    for (int i = 0; i < kFftSize; ++i) fftWin[(size_t) i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * (float) i / (float) (kFftSize - 1));
    for (int i = 0; i < kSpecCols; ++i) specIn[i] = specOut[i] = -100.0f;
    selectStage(juce::jlimit(0, nfvs::kStages - 1, (int) p.apvts.state.getProperty("stage", 0)));

    constrainer.setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    constrainer.setSizeLimits(kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    const int w = juce::jlimit(kBaseW * 3 / 4, kBaseW * 2, (int) p.apvts.state.getProperty("uiWidth", kBaseW));
    setSize(w, w * kBaseH / kBaseW);
    startTimerHz(30);
}

NFVocalSuiteAudioProcessorEditor::~NFVocalSuiteAudioProcessorEditor()
{
    for (auto* s : { &knob, &outKnob, &mixKnob, &lowKnob, &highKnob }) s->setLookAndFeel(nullptr);
}

void NFVocalSuiteAudioProcessorEditor::selectStage(int s)
{
    stage = s;
    proc.apvts.state.setProperty("stage", s, nullptr);
    knobAtt.reset();
    knobAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, sid("amt"), knob);
    knob.setDoubleClickReturnValue(true, (double) nfvs::kStageInfo[s].defAmt);
    look.accent = stageColour(s);
    repaint();
}

void NFVocalSuiteAudioProcessorEditor::resized()
{
    const float s = scale();
    auto place = [&](juce::Slider& sl, float cx, float cy, float size) {
        sl.setBounds(juce::Rectangle<float>(cx - size * 0.5f, cy - size * 0.5f, size, size).transformedBy(juce::AffineTransform::scale(s)).toNearestInt());
    };
    place(knob, 405.0f, 306.0f, 190.0f);
    place(lowKnob, 594.0f, 372.0f, 38.0f);
    place(highKnob, 648.0f, 372.0f, 38.0f);
    place(outKnob, 702.0f, 372.0f, 38.0f);
    place(mixKnob, 756.0f, 372.0f, 38.0f);
    proc.apvts.state.setProperty("uiWidth", getWidth(), nullptr);
}

int NFVocalSuiteAudioProcessorEditor::presetIndex() const
{
    const auto name = proc.apvts.state.getProperty("presetName").toString();
    for (int i = 0; i < kNumVSPresets; ++i) if (name == kVSPresets[i].name) return i;
    return -1;
}

void NFVocalSuiteAudioProcessorEditor::stepPreset(int dir)
{
    const int cur = presetIndex();
    proc.applyPreset(cur < 0 ? (dir > 0 ? 0 : kNumVSPresets - 1) : (cur + dir + kNumVSPresets) % kNumVSPresets);
}

void NFVocalSuiteAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < kNumVSPresets; ++i) m.addItem(i + 1, kVSPresets[i].name, true, i == presetIndex());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea((presetRect() * scale()).toNearestInt().translated(getScreenX(), getScreenY())),
                    [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
}

void NFVocalSuiteAudioProcessorEditor::toggleBool(const juce::String& id)
{
    if (auto* prm = proc.apvts.getParameter(id))
    {
        prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture();
    }
}

void NFVocalSuiteAudioProcessorEditor::setChoice(const juce::String& id, int index)
{
    if (auto* prm = proc.apvts.getParameter(id))
    {
        prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->convertTo0to1((float) index)); prm->endChangeGesture();
    }
}

void NFVocalSuiteAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto pos = e.position / scale();
    if (bypassRect().contains(pos)) { toggleBool("bypass"); return; }
    const auto pr = presetRect();
    if (pr.contains(pos))
    {
        if (pos.x < pr.getX() + 22.0f) stepPreset(-1);
        else if (pos.x > pr.getRight() - 22.0f) stepPreset(1);
        else showPresetMenu();
        return;
    }
    for (int i = 0; i < nfvs::kStages; ++i)
        if (railDot(i).contains(pos)) { selectStage(i); return; }
    if (prevRect().contains(pos)) { selectStage((stage + nfvs::kStages - 1) % nfvs::kStages); return; }
    if (nextRect().contains(pos)) { selectStage((stage + 1) % nfvs::kStages); return; }
    for (int i = 0; i < 3; ++i)
        if (modeRect(i).contains(pos)) { setChoice(sid("mode"), i); return; }
    if (switchRow(0).contains(pos)) { toggleBool(sid("on")); return; }
    if (switchRow(1).contains(pos)) { toggleBool(sid("a")); return; }
    if (switchRow(2).contains(pos)) { toggleBool(sid("b")); return; }
}

void NFVocalSuiteAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (juce::Rectangle<float>(22.0f, 8.0f, 90.0f, 46.0f).contains(e.position / scale())) setSize(kBaseW, kBaseH);
}

void NFVocalSuiteAudioProcessorEditor::timerCallback()
{
    bypassed = pv("bypass") > 0.5f;
    presetName = proc.apvts.state.getProperty("presetName").toString();
    const bool on = pv(sid("on")) > 0.5f;
    look.accent = on ? stageColour(stage) : juce::Colour(0xff55575e);
    float target = pv(sid("amt")) / 100.0f;
    const auto& si = nfvs::kStageInfo[stage];
    if (si.activity[0] != 0) target = juce::jlimit(0.0f, 1.0f, proc.chainRef().infoAt(stage) / 12.0f);
    act = target > act ? target : act + (target - act) * 0.15f;
    if (stage == nfvs::Fix || stage == nfvs::Tone) updateSpectrum();
    repaint();
}

// Spectrum of the signal before / after the selected stage (FIX: points 0 -> 1, TONE: points 1 -> 2), log frequency axis, in dB.
void NFVocalSuiteAudioProcessorEditor::updateSpectrum()
{
    const double fs = std::max(8000.0, proc.getSampleRate());
    const int pts[2] = { stage == nfvs::Fix ? 0 : 1, stage == nfvs::Fix ? 1 : 2 };
    for (int which = 0; which < 2; ++which)
    {
        std::vector<float> tmp((size_t) kFftSize);
        proc.chainRef().copyAnalyzer(pts[which], tmp.data(), kFftSize);
        for (int i = 0; i < kFftSize; ++i) fftData[(size_t) i] = tmp[(size_t) i] * fftWin[(size_t) i];
        std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        float* dst = which == 0 ? specIn : specOut;
        const float binHz = (float) fs / (float) kFftSize;
        for (int col = 0; col < kSpecCols; ++col)
        {
            const float f0 = 30.0f * std::pow(18000.0f / 30.0f, (float) col / (float) kSpecCols);
            const float f1 = 30.0f * std::pow(18000.0f / 30.0f, (float) (col + 1) / (float) kSpecCols);
            const int b0 = juce::jlimit(1, kFftSize / 2 - 1, (int) (f0 / binHz)), b1 = juce::jlimit(b0, kFftSize / 2 - 1, (int) std::ceil(f1 / binHz));
            float pw = 0.0f; int cnt = 0;
            for (int b = b0; b <= b1; ++b) { const float a = fftData[(size_t) b] / ((float) kFftSize * 0.25f); pw += a * a; ++cnt; }
            const float db = 10.0f * std::log10(pw / (float) std::max(1, cnt) + 1e-12f);
            dst[col] += (std::max(db, -100.0f) - dst[col]) * 0.35f;
        }
    }
}

void NFVocalSuiteAudioProcessorEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> wp, juce::Colour accent, bool modOn)
{
    const auto r = wp.reduced(14.0f, 12.0f);
    auto xOf = [&](float hz) { return r.getX() + r.getWidth() * std::log(hz / 30.0f) / std::log(18000.0f / 30.0f); };
    auto yOf = [&](float db) { return r.getBottom() - r.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 100.0f) / 70.0f); };   // -100 .. -30 dB
    g.setColour(juce::Colour(0xff17171a));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    g.setFont(font(8.5f)); 
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour(juce::Colour(0xff222226)); g.drawVerticalLine((int) xOf(hz), r.getY(), r.getBottom());
        g.setColour(kMuted); g.drawText(hz >= 1000.0f ? juce::String((int) (hz / 1000.0f)) + "k" : juce::String((int) hz), juce::Rectangle<float>(xOf(hz) + 2.0f, r.getBottom() - 11.0f, 30.0f, 10.0f), juce::Justification::left);
    }
    juce::Path pin, pout, fill;
    for (int i = 0; i < kSpecCols; ++i)
    {
        const float x = r.getX() + r.getWidth() * ((float) i + 0.5f) / (float) kSpecCols;
        if (i == 0) { pin.startNewSubPath(x, yOf(specIn[i])); pout.startNewSubPath(x, yOf(specOut[i])); }
        else { pin.lineTo(x, yOf(specIn[i])); pout.lineTo(x, yOf(specOut[i])); }
    }
    fill = pout; fill.lineTo(r.getRight(), r.getBottom()); fill.lineTo(r.getX(), r.getBottom()); fill.closeSubPath();
    const auto col = modOn ? accent : juce::Colour(0xff6b6f77);
    g.setGradientFill(juce::ColourGradient(col.withAlpha(0.30f), 0, r.getY(), col.withAlpha(0.02f), 0, r.getBottom(), false));
    g.fillPath(fill);
    g.setColour(juce::Colour(0xffc9ccd2).withAlpha(0.65f)); g.strokePath(pin, juce::PathStrokeType(1.2f));
    g.setColour(col); g.strokePath(pout, juce::PathStrokeType(2.0f));
    g.setFont(font(9.0f, true));
    g.setColour(juce::Colour(0xffc9ccd2)); g.drawText("BEFORE", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 6.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour(col); g.drawText(juce::String("AFTER ") + nfvs::kStageInfo[stage].name, juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 19.0f, 160.0f, 12.0f), juce::Justification::centredLeft);
}

// Voice positions: the original in the centre, the added copies to the left and right (heights follow the live level and the amount).
void NFVocalSuiteAudioProcessorEditor::drawVoices(juce::Graphics& g, juce::Rectangle<float> wp, juce::Colour accent, bool modOn)
{
    const auto& chain = proc.chainRef();
    const int pos = chain.histWritePos(), cols = nfvs::Chain::kHist;
    float lvl = 0.0f;
    for (int k = 1; k <= 6; ++k) lvl = std::max(lvl, chain.histAt(0, (pos - k + cols) % cols));
    const float n = juce::jlimit(0.0f, 1.0f, (nfvs::toDb(lvl) + 54.0f) / 54.0f);
    const float amt = modOn ? pv(sid("amt")) / 100.0f : 0.0f;
    const int mode = juce::jlimit(0, 2, juce::roundToInt(pv(sid("mode"))));
    const auto r = wp.reduced(20.0f, 14.0f);
    const float cx = r.getCentreX(), base = r.getBottom() - 16.0f, full = r.getHeight() - 36.0f;
    g.setColour(juce::Colour(0xff222226)); g.drawHorizontalLine((int) base, r.getX(), r.getRight());
    struct Voice { float pos, gain; };   // pos -1..1
    std::vector<Voice> vs;
    vs.push_back({ 0.0f, 1.0f });
    if (mode == 0) { vs.push_back({ -0.55f, 0.8f }); vs.push_back({ 0.55f, 0.8f }); }
    else if (mode == 1) { for (float p : { -0.8f, -0.4f, 0.4f, 0.8f }) vs.push_back({ p, 0.65f }); }
    else { vs.push_back({ -0.92f, 0.9f }); vs.push_back({ 0.92f, 0.9f }); }
    for (const auto& v : vs)
    {
        const bool centre = v.pos == 0.0f;
        const float x = cx + v.pos * (r.getWidth() * 0.5f - 20.0f);
        const float h = std::max(3.0f, full * n * (centre ? 1.0f : v.gain * (0.15f + 0.85f * amt)));
        const auto col = centre ? juce::Colour(0xffe9ebef) : (modOn ? accent : juce::Colour(0xff6b6f77));
        g.setColour(col.withAlpha(centre ? 0.30f : 0.22f)); g.fillEllipse(x - 9.0f, base - h - 9.0f, 18.0f, 18.0f);
        g.setColour(col); g.drawLine(x, base, x, base - h, centre ? 2.4f : 2.0f);
        g.fillEllipse(x - 3.5f, base - h - 3.5f, 7.0f, 7.0f);
    }
    g.setFont(font(9.0f, true)); g.setColour(kMuted);
    g.drawText("L", juce::Rectangle<float>(r.getX(), base + 2.0f, 20.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText("R", juce::Rectangle<float>(r.getRight() - 20.0f, base + 2.0f, 20.0f, 12.0f), juce::Justification::centredRight);
    g.drawText("CENTRE", juce::Rectangle<float>(cx - 30.0f, base + 2.0f, 60.0f, 12.0f), juce::Justification::centred);
    g.setColour(accent); g.drawText("VOICES", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 6.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
}

void NFVocalSuiteAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(scale()));
    const float W = (float) kBaseW, H = (float) kBaseH;
    const auto& si = nfvs::kStageInfo[stage];
    const auto accent = stageColour(stage);
    const bool modOn = pv(sid("on")) > 0.5f;

    // chassis
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1d1d20), 0, 0, juce::Colour(0xff0f0f11), 0, H, false));
    g.fillRoundedRectangle(0, 0, W, H, 10.0f);
    g.setColour(kLine); g.drawRoundedRectangle(6.5f, 6.5f, W - 13.0f, H - 13.0f, 7.0f, 1.0f);
    g.setColour(kOff);
    for (float x : { 18.0f, W - 18.0f }) for (float y : { 18.0f, H - 18.0f }) g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);

    // header
    if (logo.isValid())
        g.drawImage(logo, juce::Rectangle<float>(26.0f, 10.0f, 80.0f, 44.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    g.setColour(kText); g.setFont(font(19.0f, true));
    g.drawText("V O C A L   S U I T E", juce::Rectangle<float>(116.0f, 20.0f, 300.0f, 26.0f), juce::Justification::centredLeft);

    const auto bp = bypassRect();
    g.setColour(juce::Colour(0xff25252a)); g.fillRoundedRectangle(bp, 5.0f);
    g.setColour(kLine); g.drawRoundedRectangle(bp.reduced(0.5f), 5.0f, 1.0f);
    g.setColour(bypassed ? juce::Colour(0xffe8863a) : kOff); g.fillEllipse(bp.getX() + 9.0f, bp.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour(bypassed ? kText : kMuted); g.setFont(font(10.0f));
    g.drawText("BYPASS", bp.withTrimmedLeft(24.0f), juce::Justification::centredLeft);

    const auto pr = presetRect();
    g.setColour(juce::Colour(0xff0e0e10)); g.fillRoundedRectangle(pr, 5.0f);
    g.setColour(kLine); g.drawRoundedRectangle(pr.reduced(0.5f), 5.0f, 1.0f);
    g.setColour(juce::Colour(0xffd9b26a)); g.setFont(font(13.0f, true));
    g.drawText("<", pr.withWidth(22.0f), juce::Justification::centred);
    g.drawText(">", pr.withTrimmedLeft(pr.getWidth() - 22.0f), juce::Justification::centred);
    g.setColour(kText); g.setFont(font(11.0f));
    g.drawText(presetName.isEmpty() ? juce::String("Init") : presetName, pr.reduced(24.0f, 0.0f), juce::Justification::centred);

    // ---- before / after waveform
    const auto wp = waveRect();
    g.setColour(kDark); g.fillRoundedRectangle(wp, 8.0f);
    g.setColour(kLine); g.drawRoundedRectangle(wp.reduced(0.5f), 8.0f, 1.0f);
    const auto& chain = proc.chainRef();
    if (stage == nfvs::Fix || stage == nfvs::Tone) drawSpectrum(g, wp, accent, modOn);
    else if (stage == nfvs::Voices) drawVoices(g, wp, accent, modOn);
    else
    {
    const int pos = chain.histWritePos(), cols = nfvs::Chain::kHist;
    for (int lane = 0; lane < 2; ++lane)
    {
        const auto lr = juce::Rectangle<float>(wp.getX() + 10.0f, wp.getY() + 8.0f + 60.0f * (float) lane, wp.getWidth() - 20.0f, 54.0f);
        const int tap = lane == 0 ? 0 : stage + 1;
        const float cw = lr.getWidth() / (float) cols, cy = lr.getCentreY();
        g.setColour(juce::Colour(0xff1d1d21)); g.drawLine(lr.getX(), cy, lr.getRight(), cy, 1.0f);
        for (int i = 0; i < cols; ++i)
        {
            const float v = chain.histAt(tap, (pos + i) % cols);
            const float n = juce::jlimit(0.0f, 1.0f, (nfvs::toDb(v) + 54.0f) / 54.0f);
            const float hh = std::max(0.8f, n * lr.getHeight() * 0.5f);
            const float age = 0.30f + 0.70f * (float) i / (float) cols;
            g.setColour((lane == 0 ? juce::Colour(0xffc9ccd2) : (modOn ? accent : juce::Colour(0xff6b6f77))).withAlpha(age));
            g.fillRoundedRectangle(lr.getX() + (float) i * cw, cy - hh, std::max(1.0f, cw * 0.68f), hh * 2.0f, 0.8f);
        }
    }
    g.setFont(font(9.0f, true));
    g.setColour(kMuted); g.drawText("ORIGINAL", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 8.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour(accent); g.drawText(juce::String("AFTER ") + si.name, juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 68.0f, 200.0f, 12.0f), juce::Justification::centredLeft);
    }

    // ---- left panel: style
    auto panel = [&](juce::Rectangle<float> r, const char* title) {
        g.setColour(kPanel); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
        g.setColour(kMuted); g.setFont(font(10.0f, true));
        g.drawText(title, r.withHeight(24.0f).withTrimmedLeft(14.0f).withTrimmedTop(6.0f), juce::Justification::centredLeft);
    };
    panel(leftPanel(), "S T Y L E");
    const int mode = juce::jlimit(0, 2, juce::roundToInt(pv(sid("mode"))));
    for (int i = 0; i < 3; ++i)
    {
        const auto r = modeRect(i);
        const bool sel = i == mode;
        g.setColour(sel ? accent.withAlpha(0.16f) : juce::Colour(0xff131316)); g.fillRoundedRectangle(r, 15.0f);
        g.setColour(sel ? accent : kLine); g.drawRoundedRectangle(r.reduced(0.5f), 15.0f, sel ? 1.4f : 1.0f);
        g.setColour(sel ? accent : kOff); g.fillEllipse(r.getX() + 13.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour(sel ? kText : kMuted); g.setFont(font(11.0f, sel));
        g.drawText(si.modes[i], r.withTrimmedLeft(30.0f), juce::Justification::centredLeft);
    }
    // activity meter
    {
        const auto lp = leftPanel();
        g.setColour(kMuted); g.setFont(font(8.5f, true));
        const bool isInfo = si.activity[0] != 0;
        g.drawText(isInfo ? si.activity : "AMOUNT", juce::Rectangle<float>(lp.getX() + 14.0f, lp.getBottom() - 44.0f, 150.0f, 12.0f), juce::Justification::centredLeft);
        const auto bar = juce::Rectangle<float>(lp.getX() + 14.0f, lp.getBottom() - 26.0f, lp.getWidth() - 28.0f, 8.0f);
        g.setColour(juce::Colour(0xff0b0b0c)); g.fillRoundedRectangle(bar, 4.0f);
        g.setColour(modOn ? accent : juce::Colour(0xff55575e)); g.fillRoundedRectangle(bar.withWidth(std::max(6.0f, bar.getWidth() * act)), 4.0f);
        if (isInfo)
        {
            g.setColour(kText); g.setFont(font(9.0f));
            g.drawText("-" + juce::String(chain.infoAt(stage), 1) + " dB", juce::Rectangle<float>(lp.getRight() - 74.0f, lp.getBottom() - 44.0f, 60.0f, 12.0f), juce::Justification::centredRight);
        }
    }

    // ---- right panel: options
    panel(rightPanel(), "O P T I O N S");
    {
        g.setFont(font(10.5f));
        const juce::String labels[3] = { "MODULE ON", si.toggleA, si.toggleB };
        const bool states[3] = { modOn, pv(sid("a")) > 0.5f, pv(sid("b")) > 0.5f };
        for (int row = 0; row < 3; ++row)
        {
            const auto r = switchRow(row);
            g.setColour(states[row] ? kText : kMuted);
            g.drawText(labels[row], r.withTrimmedRight(50.0f), juce::Justification::centredLeft);
            drawSwitch(g, juce::Rectangle<float>(r.getRight() - 40.0f, r.getY() + 3.0f, 38.0f, 18.0f), states[row], row == 0 ? juce::Colour(0xffd9b26a) : accent);
        }
        g.setColour(kMuted); g.setFont(font(8.5f));
        const std::pair<juce::Slider*, const char*> kn[] = { { &lowKnob, "LOW CUT" }, { &highKnob, "HIGH CUT" }, { &outKnob, "OUTPUT" }, { &mixKnob, "MIX" } };
        for (const auto& k : kn)
            g.drawText(k.second, juce::Rectangle<float>((float) k.first->getBounds().getCentreX() / scale() - 27.0f, 396.0f, 54.0f, 12.0f), juce::Justification::centred);
    }

    // ---- centre: stage name, hint
    g.setColour(modOn ? accent : juce::Colour(0xff6b6f77)); g.setFont(font(14.0f, true));
    juce::String spaced; for (auto ch : juce::String(si.name)) spaced << ch << " ";
    g.drawText(spaced.trim(), juce::Rectangle<float>(305.0f, 396.0f, 200.0f, 18.0f), juce::Justification::centred);
    g.setColour(kMuted); g.setFont(font(9.5f));
    g.drawText(si.hint, juce::Rectangle<float>(250.0f, 413.0f, 310.0f, 13.0f), juce::Justification::centred);

    // ---- stage rail
    g.setColour(kLine); g.drawLine(railDot(0).getCentreX(), 448.0f, railDot(nfvs::kStages - 1).getCentreX(), 448.0f, 2.0f);
    for (int i = 0; i < nfvs::kStages; ++i)
    {
        const auto r = railDot(i);
        const bool on = pv(NFVocalSuiteAudioProcessor::stageId(i, "on")) > 0.5f, sel = i == stage;
        const auto col = stageColour(i);
        const float cx = r.getCentreX();
        if (sel)
        {
            g.setColour(col.withAlpha(0.18f)); g.fillEllipse(cx - 15.0f, 448.0f - 15.0f, 30.0f, 30.0f);
            g.setColour(col); g.fillEllipse(cx - 8.0f, 448.0f - 8.0f, 16.0f, 16.0f);
            g.setColour(juce::Colours::white.withAlpha(0.35f)); g.fillEllipse(cx - 4.0f, 448.0f - 6.0f, 6.0f, 5.0f);
        }
        else
        {
            g.setColour(on ? col : kOff); g.fillEllipse(cx - 5.0f, 448.0f - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colour(0xff1b1b1e)); g.drawEllipse(cx - 5.0f, 448.0f - 5.0f, 10.0f, 10.0f, 1.5f);
        }
        g.setColour(sel ? kText : (on ? kMuted : juce::Colour(0xff55575e))); g.setFont(font(9.0f, sel));
        g.drawText(nfvs::kStageInfo[i].name, juce::Rectangle<float>(cx - 40.0f, 462.0f, 80.0f, 12.0f), juce::Justification::centred);
    }
    g.setColour(juce::Colour(0xffd9b26a)); g.setFont(font(18.0f, true));
    g.drawText("<", prevRect(), juce::Justification::centred);
    g.drawText(">", nextRect(), juce::Justification::centred);

    g.setColour(juce::Colour(0xff6b6f77)); g.setFont(font(9.0f));
    g.drawText("V" JucePlugin_VersionString, juce::Rectangle<float>(W * 0.5f - 30.0f, H - 19.0f, 60.0f, 12.0f), juce::Justification::centred);
}
