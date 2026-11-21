#include "PluginEditor.h"
#include "NFVocalSuiteBinaryData.h"
#include "Presets.h"
#include <complex>

namespace
{
const juce::Colour kPanel(0xff1b1b1e), kLine(0xff2e2e33), kText(0xfff2efe8), kMuted(0xff8b8f97), kOff(0xff3a3a40), kDark(0xff0c0c0e);
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f, kEnd = juce::MathConstants<float>::pi * 2.75f;
juce::Font font(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Colour stageColour(int s) { return juce::Colour(nfvs::kStageInfo[s].colour); }

}

// Hardware knob: printed scale, dark skirt, smooth brushed-aluminium cap (no machined rings), coloured pointer.
void NFVocalSuiteAudioProcessorEditor::KnobLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                                                  float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const bool big = std::min(w, h) > 100;
    const float R = std::min(w, h) * 0.5f - 2.0f;
    const float a = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0;
    juce::ignoreUnused(bipolar);

    const int nT = big ? 41 : 21;
    for (int i = 0; i < nT; ++i)
    {
        const float t = (float) i / (float) (nT - 1), ang = start + t * (end - start), sx = std::sin(ang), cy = -std::cos(ang);
        const bool major = i % (big ? 5 : 5) == 0;
        const float r1 = R * (big ? 0.90f : 0.88f), r2 = R * (major ? 1.0f : 0.96f);
        const bool here = std::abs(t - pos) <= 0.5f / (float) (nT - 1);
        g.setColour(here ? accent : juce::Colour(major ? 0xff9a9da5 : 0xff62656c));
        g.drawLine(c.x + sx * r1, c.y + cy * r1, c.x + sx * r2, c.y + cy * r2, here ? (big ? 3.0f : 2.2f) : (major ? 1.5f : 1.0f));
    }
    if (big && style != 0)   // sun / full moon instead of the metal cap
    {
        const float pi = juce::MathConstants<float>::pi;
        const float bodyR = R * (style == 1 ? 0.56f : 0.66f);
        if (style == 1)
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x70ffb02e), c.x, c.y, juce::Colour(0x00ff8a1f), c.x + R * 0.95f, c.y, true));
            g.fillEllipse(c.x - R * 0.95f, c.y - R * 0.95f, R * 1.9f, R * 1.9f);
            for (int k = 0; k < 24; ++k)   // rays: long and short, alternating
            {
                const float ang = (float) k * 2.0f * pi / 24.0f + 0.07f, half = (k % 2 == 0 ? 0.075f : 0.06f);
                const float r0 = bodyR * 1.08f, r1 = R * (k % 2 == 0 ? 0.86f : 0.74f);
                juce::Path ray;
                ray.startNewSubPath(c.x + std::sin(ang - half) * r0, c.y - std::cos(ang - half) * r0);
                ray.lineTo(c.x + std::sin(ang) * r1, c.y - std::cos(ang) * r1);
                ray.lineTo(c.x + std::sin(ang + half) * r0, c.y - std::cos(ang + half) * r0);
                ray.closeSubPath();
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffe27a), c.x + std::sin(ang) * r0, c.y - std::cos(ang) * r0, juce::Colour(0xffff8a1f), c.x + std::sin(ang) * r1, c.y - std::cos(ang) * r1, false));
                g.fillPath(ray);
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffff6c4), c.x - bodyR * 0.35f, c.y - bodyR * 0.4f, juce::Colour(0xffff9a1f), c.x + bodyR * 0.9f, c.y + bodyR * 0.95f, true));
            g.fillEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour(juce::Colour(0x55fff2b0)); g.drawEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);
        }
        else
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x60cfe0ff), c.x, c.y, juce::Colour(0x00a8c4ff), c.x + R * 0.95f, c.y, true));
            g.fillEllipse(c.x - R * 0.95f, c.y - R * 0.95f, R * 1.9f, R * 1.9f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffaf8ef), c.x - bodyR * 0.5f, c.y - bodyR * 0.6f, juce::Colour(0xffa9a79d), c.x + bodyR * 0.8f, c.y + bodyR * 0.9f, true));
            g.fillEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.saveState();
            juce::Path disc; disc.addEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f); g.reduceClipRegion(disc);
            static const float maria[3][3] = { { -0.30f, -0.10f, 0.50f }, { 0.28f, 0.32f, 0.42f }, { 0.10f, -0.52f, 0.32f } };
            for (const auto& m : maria)
            {
                const float mx = c.x + m[0] * bodyR, my = c.y + m[1] * bodyR, mr = m[2] * bodyR;
                g.setGradientFill(juce::ColourGradient(juce::Colour(0x3a6e6c66), mx, my, juce::Colour(0x006e6c66), mx + mr, my, true));
                g.fillEllipse(mx - mr, my - mr, mr * 2.0f, mr * 2.0f);
            }
            static const float craters[9][3] = { { -0.35f, -0.30f, 0.18f }, { 0.30f, -0.45f, 0.12f }, { 0.45f, 0.10f, 0.20f }, { -0.10f, 0.35f, 0.15f }, { -0.55f, 0.25f, 0.10f },
                                                 { 0.10f, -0.05f, 0.09f }, { 0.25f, 0.58f, 0.08f }, { -0.20f, -0.65f, 0.07f }, { 0.65f, -0.22f, 0.07f } };
            for (const auto& cr : craters)
            {
                const float cx2 = c.x + cr[0] * bodyR, cy2 = c.y + cr[1] * bodyR, r2 = cr[2] * bodyR;
                g.setColour(juce::Colour(0x4a77756d)); g.fillEllipse(cx2 - r2, cy2 - r2, r2 * 2.0f, r2 * 2.0f);
                g.setColour(juce::Colour(0x55ffffff)); g.drawEllipse(cx2 - r2 + 0.6f, cy2 - r2 + 0.9f, r2 * 2.0f, r2 * 2.0f, 1.0f);   // lit rim on the lower side
                g.setColour(juce::Colour(0x33000000)); g.drawEllipse(cx2 - r2, cy2 - r2, r2 * 2.0f, r2 * 2.0f, 0.8f);
            }
            g.restoreState();
        }
        const float a2 = start + pos * (end - start), sx2 = std::sin(a2), cy2 = -std::cos(a2);
        g.setColour(style == 1 ? juce::Colour(0xff7a2d00) : juce::Colour(0xff3c4150));
        g.drawLine(c.x + sx2 * bodyR * 0.30f, c.y + cy2 * bodyR * 0.30f, c.x + sx2 * bodyR * 0.88f, c.y + cy2 * bodyR * 0.88f, 5.0f);
        return;
    }
    const float skirtR = R * (big ? 0.80f : 0.78f), capR = R * (big ? 0.64f : 0.62f);
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(c.x - skirtR, c.y - skirtR + 3.0f, skirtR * 2.0f, skirtR * 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4c52), c.x - skirtR, c.y - skirtR, juce::Colour(0xff17181a), c.x + skirtR, c.y + skirtR, false));
    g.fillEllipse(c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour(juce::Colour(0x55000000)); g.drawEllipse(c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f, 1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f3f5), c.x - capR * 0.55f, c.y - capR * 0.65f, juce::Colour(0xff80838b), c.x + capR * 0.75f, c.y + capR * 0.85f, true));
    g.fillEllipse(c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour(0x66ffffff)); g.drawEllipse(c.x - capR + 0.5f, c.y - capR + 0.5f, capR * 2.0f - 1.0f, capR * 2.0f - 1.0f, 1.0f);
    const float sx = std::sin(a), cy = -std::cos(a);
    g.setColour(big ? accent.darker(0.15f) : juce::Colour(0xff202226));
    g.drawLine(c.x + sx * capR * 0.30f, c.y + cy * capR * 0.30f, c.x + sx * capR * 0.90f, c.y + cy * capR * 0.90f, big ? 5.0f : 2.8f);
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
    setup(atkKnob, smallLook, 200); setup(relKnob, smallLook, 200); setup(ratKnob, smallLook, 200);
    for (auto* s : { &atkKnob, &relKnob, &ratKnob }) s->setDoubleClickReturnValue(true, 50.0);
    atkAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "ctlatk", atkKnob);
    relAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "ctlrel", relKnob);
    ratAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "ctlratio", ratKnob);
    lowKnob.setDoubleClickReturnValue(true, 20.0);
    highKnob.setDoubleClickReturnValue(true, 20000.0);
    lowAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "lowcut", lowKnob);
    highAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "highcut", highKnob);
    outKnob.setDoubleClickReturnValue(true, 0.0);
    mixKnob.setDoubleClickReturnValue(true, 100.0);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output", outKnob);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "mix", mixKnob);
    brushed = juce::Image(juce::Image::ARGB, kBaseW, kBaseH, true);   // brushed-metal grain, drawn once
    {
        juce::Graphics bg(brushed); juce::Random rnd(11);
        for (int i = 0; i < 2800; ++i)
        {
            const int y = rnd.nextInt(kBaseH), x = rnd.nextInt(kBaseW), len = 40 + rnd.nextInt(380);
            const float a = 0.015f + rnd.nextFloat() * 0.035f;
            bg.setColour(rnd.nextBool() ? juce::Colours::white.withAlpha(a) : juce::Colours::black.withAlpha(a * 1.4f));
            bg.fillRect(x, y, len, 1);
        }
    }
    fftWin.resize(kFftSize); fftData.resize(kFftSize * 2);
    for (int i = 0; i < kFftSize; ++i) fftWin[(size_t) i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * (float) i / (float) (kFftSize - 1));
    for (int i = 0; i < kSpecCols; ++i) specIn[i] = specOut[i] = -100.0f;
    selectStage(juce::jlimit(0, nfvs::kStages - 1, (int) p.apvts.state.getProperty("stage", 0)));

    look.style = juce::jlimit(0, 2, (int) p.apvts.state.getProperty("knobStyle", 1));
    setupValueTexts();

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
    for (auto* s : { &knob, &outKnob, &mixKnob, &lowKnob, &highKnob, &atkKnob, &relKnob, &ratKnob }) s->setLookAndFeel(nullptr);
}

// Slider attachments install their own text functions, so ours are set after every attach: real units on every knob.
void NFVocalSuiteAudioProcessorEditor::setupValueTexts()
{
    auto ctlMode = [this]() { return juce::jlimit(0, 2, juce::roundToInt(pv(NFVocalSuiteAudioProcessor::stageId(nfvs::Control, "mode")))); };
    auto ms = [](double v) { return v < 10.0 ? juce::String(v, 1) + " ms" : juce::String(juce::roundToInt(v)) + " ms"; };
    atkKnob.textFromValueFunction = [ctlMode, ms](double v) { static const double b[3] = { 15.0, 24.0, 6.0 }; return ms(b[ctlMode()] * std::pow(4.0, (v - 50.0) / 50.0)); };
    relKnob.textFromValueFunction = [ctlMode, ms](double v) { static const double b[3] = { 220.0, 130.0, 90.0 }; return ms(b[ctlMode()] * std::pow(4.0, (v - 50.0) / 50.0)); };
    ratKnob.textFromValueFunction = [ctlMode](double v) {
        static const double b[3] = { 2.2, 3.5, 6.0 };
        const double r = juce::jlimit(1.2, 20.0, b[ctlMode()] * std::pow(2.0, (v - 50.0) / 50.0));
        return (r >= 10.0 ? juce::String(juce::roundToInt(r)) : juce::String(r, 1)) + ":1";
    };
    lowKnob.textFromValueFunction = [](double v) { return v < 20.5 ? juce::String("Off") : juce::String(juce::roundToInt(v)) + " Hz"; };
    highKnob.textFromValueFunction = [](double v) { return v > 19900.0 ? juce::String("Off") : (v >= 1000.0 ? juce::String(v / 1000.0, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"); };
    outKnob.textFromValueFunction = [](double v) { return (v > 0.04 ? "+" : "") + juce::String(v, 1) + " dB"; };
    mixKnob.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " %"; };
    knob.textFromValueFunction = [this](double v) {
        auto s = juce::String(juce::roundToInt(v)) + " %";
        if (stage == nfvs::Control) s += "   threshold " + juce::String(juce::roundToInt(-9.0 - 22.0 * v / 100.0)) + " dB";
        return s;
    };
}

void NFVocalSuiteAudioProcessorEditor::selectStage(int s)
{
    stage = s;
    proc.apvts.state.setProperty("stage", s, nullptr);
    knobAtt.reset();
    knobAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, sid("amt"), knob);
    knob.setDoubleClickReturnValue(true, (double) nfvs::kStageInfo[s].defAmt);
    setupValueTexts();
    look.accent = stageColour(s);
    for (auto* k : { &atkKnob, &relKnob, &ratKnob }) k->setVisible(s == nfvs::Control);
    repaint();
}

void NFVocalSuiteAudioProcessorEditor::resized()
{
    const float s = scale();
    auto place = [&](juce::Slider& sl, float cx, float cy, float size) {
        sl.setBounds(juce::Rectangle<float>(cx - size * 0.5f, cy - size * 0.5f, size, size).transformedBy(juce::AffineTransform::scale(s)).toNearestInt());
    };
    place(knob, 338.0f, 368.0f, 176.0f);
    place(atkKnob, 586.0f, 312.0f, 42.0f);
    place(relKnob, 666.0f, 312.0f, 42.0f);
    place(ratKnob, 746.0f, 312.0f, 42.0f);
    place(lowKnob, 574.0f, 414.0f, 38.0f);
    place(highKnob, 636.0f, 414.0f, 38.0f);
    place(outKnob, 698.0f, 414.0f, 38.0f);
    place(mixKnob, 760.0f, 414.0f, 38.0f);
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
    if (knobStyleRect().contains(pos))   // metal -> sun -> full moon
    {
        look.style = (look.style + 1) % 3;
        proc.apvts.state.setProperty("knobStyle", look.style, nullptr);
        knob.repaint(); repaint(); return;
    }
    if (stage == nfvs::Eq && waveRect().contains(pos))
    {
        const int b = eqNodeAt(pos);
        if (b >= 0)
        {
            dragBand = selBand = b;
            for (const char* k : { "f", "g" }) if (auto* prm = proc.apvts.getParameter("eq" + juce::String(b + 1) + k)) prm->beginChangeGesture();
            setEqFromPos(b, pos);
        }
        return;
    }
    if (stage == nfvs::Voices && waveRect().contains(pos))
    {
        if (auto* prm = proc.apvts.getParameter("vspread")) prm->beginChangeGesture();
        draggingSpread = true; setSpreadFromX(pos.x); return;
    }
    const auto pr = presetRect();
    if (pr.contains(pos))
    {
        if (pos.x < pr.getX() + 22.0f) stepPreset(-1);
        else if (pos.x > pr.getRight() - 22.0f) stepPreset(1);
        else showPresetMenu();
        return;
    }
    for (int i = 0; i < nfvs::kStages; ++i)   // module list: the LED switches the module on / off, the rest of the row selects it
        if (moduleRow(i).contains(pos)) { if (pos.x < moduleRow(i).getX() + 30.0f) toggleBool(NFVocalSuiteAudioProcessor::stageId(i, "on")); else selectStage(i); return; }
    if (stage == nfvs::Harmony)
    {
        static const int kColToHv[7] = { 0, 1, -1, 2, 3, 4, 5 };
        for (int c = 0; c < 7 && stage == nfvs::Harmony; ++c)
            if (kColToHv[c] >= 0 && harmChip(c).contains(pos)) { toggleBool("hv" + juce::String(kColToHv[c] + 1)); return; }
        if (keyRect().contains(pos))
        {
            juce::PopupMenu m;
            static const char* kKeys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int cur = juce::roundToInt(pv(keyId()));
            for (int k = 0; k < 12; ++k) m.addItem(k + 1, kKeys[k], true, k == cur);
            m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea((keyRect() * scale()).toNearestInt().translated(getScreenX(), getScreenY())),
                            [this](int r) { if (r > 0) setChoice(keyId(), r - 1); });
            return;
        }
    }
    for (int i = 0; i < 3; ++i)
        if (modeRect(i).contains(pos)) { setChoice(sid("mode"), i); if (stage == nfvs::Eq) applyEqCurve(i); return; }
    if (switchRow(1).contains(pos)) { toggleBool(sid("a")); return; }
    if (switchRow(2).contains(pos)) { toggleBool(sid("b")); return; }
}

// Dragging in the VOICES display sets how far the left / right copies open (distance of the dots from the centre).
void NFVocalSuiteAudioProcessorEditor::setSpreadFromX(float x)
{
    const auto wp = waveRect();
    const float half = (wp.getWidth() - 40.0f) * 0.5f - 20.0f;
    const float v = juce::jlimit(0.0f, 1.0f, std::abs(x - wp.getCentreX()) / (half * 0.92f));
    if (auto* prm = proc.apvts.getParameter("vspread")) prm->setValueNotifyingHost(prm->convertTo0to1(v * 100.0f));
}

void NFVocalSuiteAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragBand >= 0) setEqFromPos(dragBand, e.position / scale());
    else if (draggingSpread) setSpreadFromX(e.position.x / scale());
}

void NFVocalSuiteAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    if (dragBand >= 0)
    {
        for (const char* k : { "f", "g" }) if (auto* prm = proc.apvts.getParameter("eq" + juce::String(dragBand + 1) + k)) prm->endChangeGesture();
        dragBand = -1;
    }
    if (draggingSpread)
    {
        draggingSpread = false;
        if (auto* prm = proc.apvts.getParameter("vspread")) prm->endChangeGesture();
    }
}

void NFVocalSuiteAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (stage != nfvs::Eq || !waveRect().contains(e.position / scale())) return;
    int b = eqNodeAt(e.position / scale());
    if (b < 0) b = selBand;
    if (b != 1 && b != 2) return;   // only the bells have a Q
    selBand = b;
    if (auto* prm = proc.apvts.getParameter("eq" + juce::String(b + 1) + "q"))
    {
        const float q = juce::jlimit(0.3f, 8.0f, pv("eq" + juce::String(b + 1) + "q") * std::exp(w.deltaY * 0.6f));
        prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->convertTo0to1(q)); prm->endChangeGesture();
    }
}

void NFVocalSuiteAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (stage == nfvs::Eq && waveRect().contains(e.position / scale()))
    {
        const int b = eqNodeAt(e.position / scale());
        if (b >= 0)
            for (const char* k : { "f", "g", "q" })
                if (auto* prm = proc.apvts.getParameter("eq" + juce::String(b + 1) + k)) { prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->getDefaultValue()); prm->endChangeGesture(); }
        return;
    }
    if (stage == nfvs::Voices && waveRect().contains(e.position / scale()))
    {
        if (auto* prm = proc.apvts.getParameter("vspread")) { prm->beginChangeGesture(); prm->setValueNotifyingHost(1.0f); prm->endChangeGesture(); }
        return;
    }
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
    if (stage == nfvs::Fix || stage == nfvs::Eq || stage == nfvs::Tone) updateSpectrum();
    repaint();
}

// Spectrum of the signal before / after the selected stage (FIX: points 0 -> 1, TONE: points 1 -> 2), log frequency axis, in dB.
void NFVocalSuiteAudioProcessorEditor::updateSpectrum()
{
    const double fs = std::max(8000.0, proc.getSampleRate());
    const int pts[2] = { stage == nfvs::Fix ? 0 : stage == nfvs::Eq ? 1 : 2, stage == nfvs::Fix ? 1 : stage == nfvs::Eq ? 2 : 3 };
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
    const float spr = pv("vspread") / 100.0f * 0.92f;
    if (mode == 0) { vs.push_back({ -spr, 0.8f }); vs.push_back({ spr, 0.8f }); }
    else if (mode == 1) { for (float p : { -1.0f, -0.55f, 0.55f, 1.0f }) vs.push_back({ p * spr, 0.65f }); }
    else { vs.push_back({ -spr, 0.9f }); vs.push_back({ spr, 0.9f }); }
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
    g.setColour(kMuted); g.drawText("Drag sideways to set how far the L / R copies open", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 20.0f, 360.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour(kText); g.setFont(font(11.0f, true));
    g.drawText("WIDTH  " + juce::String(juce::roundToInt(pv("vspread"))) + " %", juce::Rectangle<float>(wp.getRight() - 150.0f, wp.getY() + 6.0f, 136.0f, 14.0f), juce::Justification::centredRight);
}

void NFVocalSuiteAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(scale()));
    const float W = (float) kBaseW, H = (float) kBaseH;
    const auto& si = nfvs::kStageInfo[stage];
    const auto accent = stageColour(stage);
    const bool modOn = pv(sid("on")) > 0.5f;
    const auto& chain = proc.chainRef();

    auto recess = [&](juce::Rectangle<float> r, float rad) {   // sunken panel
        g.setColour(juce::Colour(0xff0e0f11)); g.fillRoundedRectangle(r, rad);
        g.setColour(juce::Colour(0xaa000000)); g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.2f);
        g.setColour(juce::Colour(0x1cffffff)); g.drawLine(r.getX() + rad, r.getBottom() + 0.5f, r.getRight() - rad, r.getBottom() + 0.5f, 1.0f);
    };
    auto textW = [](const juce::Font& f, const juce::String& s) { juce::GlyphArrangement ga; ga.addLineOfText(f, s, 0.0f, 0.0f); return ga.getBoundingBox(0, -1, true).getWidth(); };
    auto glow = [&](float cx, float cy, float rad, juce::Colour col) { g.setColour(col.withAlpha(0.22f)); g.fillEllipse(cx - rad * 2.0f, cy - rad * 2.0f, rad * 4.0f, rad * 4.0f); g.setColour(col); g.fillEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f); };

    // ---- faceplate: brushed graphite with rack screws
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3b3c41), 0, 0, juce::Colour(0xff1c1d20), 0, H, false));
    g.fillRoundedRectangle(0, 0, W, H, 12.0f);
    if (brushed.isValid())
    {
        g.saveState(); juce::Path clip; clip.addRoundedRectangle(0.0f, 0.0f, W, H, 12.0f); g.reduceClipRegion(clip);
        g.drawImage(brushed, juce::Rectangle<float>(0, 0, W, H)); g.restoreState();
    }
    g.setColour(juce::Colour(0x33ffffff)); g.drawRoundedRectangle(0.5f, 0.5f, W - 1.0f, H - 1.0f, 12.0f, 1.0f);
    g.setColour(juce::Colour(0x88000000)); g.drawRoundedRectangle(3.5f, 3.5f, W - 7.0f, H - 7.0f, 9.0f, 1.2f);
    for (float sx : { 14.0f, W - 14.0f }) for (float sy : { 14.0f, H - 14.0f })
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd0d3d9), sx - 3.0f, sy - 3.0f, juce::Colour(0xff5a5d64), sx + 4.0f, sy + 4.0f, true));
        g.fillEllipse(sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0x99000000)); g.drawEllipse(sx - 5.0f, sy - 5.0f, 10.0f, 10.0f, 1.0f);
        g.setColour(juce::Colour(0xff25262a)); g.drawLine(sx - 3.2f, sy + 1.8f, sx + 3.2f, sy - 1.8f, 1.6f);
    }

    // ---- header
    if (logo.isValid())
        g.drawImage(logo, juce::Rectangle<float>(26.0f, 10.0f, 80.0f, 44.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    g.setColour(juce::Colour(0xfff0ede6)); g.setFont(font(19.0f, true));
    g.drawText("V O C A L   S U I T E", juce::Rectangle<float>(116.0f, 20.0f, 300.0f, 26.0f), juce::Justification::centredLeft);

    {
        const auto ks = knobStyleRect();
        recess(ks, 5.0f);
        static const char* kNames[3] = { "METAL", "SUN", "MOON" };
        g.setColour(kMuted); g.setFont(font(9.0f, true));
        g.drawText("KNOB", ks.withTrimmedLeft(10.0f), juce::Justification::centredLeft);
        g.setColour(look.style == 1 ? juce::Colour(0xffffb02e) : look.style == 2 ? juce::Colour(0xffcfe0ff) : kText);
        g.drawText(kNames[look.style], ks.withTrimmedRight(10.0f), juce::Justification::centredRight);
    }

    const auto bp = bypassRect();
    recess(bp, 5.0f);
    glow(bp.getX() + 13.0f, bp.getCentreY(), 3.5f, bypassed ? juce::Colour(0xffe8863a) : juce::Colour(0xff35363b));
    g.setColour(bypassed ? kText : kMuted); g.setFont(font(10.0f, true));
    g.drawText("BYPASS", bp.withTrimmedLeft(26.0f), juce::Justification::centredLeft);

    const auto pr = presetRect();
    recess(pr, 5.0f);
    g.setColour(juce::Colour(0xffd9b26a)); g.setFont(font(13.0f, true));
    g.drawText("<", pr.withWidth(22.0f), juce::Justification::centred);
    g.drawText(">", pr.withTrimmedLeft(pr.getWidth() - 22.0f), juce::Justification::centred);
    g.setColour(kText); g.setFont(font(11.0f));
    g.drawText(presetName.isEmpty() ? juce::String("Init") : presetName, pr.reduced(24.0f, 0.0f), juce::Justification::centred);

    // ---- module list (signal flow, top to bottom). The LED switches a module on / off.
    recess(juce::Rectangle<float>(14.0f, 65.0f, 158.0f, 386.0f), 8.0f);
    for (int i = 0; i < nfvs::kStages; ++i)
    {
        const auto r = moduleRow(i);
        const bool on = pv(NFVocalSuiteAudioProcessor::stageId(i, "on")) > 0.5f, sel = i == stage;
        const auto col = stageColour(i);
        if (sel)
        {
            g.setGradientFill(juce::ColourGradient(col.withAlpha(0.26f), r.getX(), r.getY(), col.withAlpha(0.06f), r.getRight(), r.getY(), false));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(col.withAlpha(0.9f)); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.2f);
        }
        if (on) glow(r.getX() + 15.0f, r.getCentreY(), 3.6f, col);
        else { g.setColour(juce::Colour(0xff2b2d31)); g.fillEllipse(r.getX() + 11.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f); g.setColour(juce::Colour(0xff0a0a0b)); g.drawEllipse(r.getX() + 11.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f, 1.0f); }
        g.setColour(sel ? kText : (on ? juce::Colour(0xffb4b7be) : juce::Colour(0xff5b5e65))); g.setFont(font(10.5f, sel));
        g.drawText(nfvs::kStageInfo[i].name, r.withTrimmedLeft(30.0f).withTrimmedRight(26.0f), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff595c63)); g.setFont(font(8.5f));
        g.drawText(juce::String(i + 1).paddedLeft('0', 2), r.withTrimmedRight(8.0f), juce::Justification::centredRight);
    }

    // ---- display (bezel + glass)
    const auto wp = waveRect();
    g.setColour(juce::Colour(0xff0a0b0c)); g.fillRoundedRectangle(wp.expanded(6.0f), 10.0f);
    g.setColour(juce::Colour(0x2cffffff)); g.drawRoundedRectangle(wp.expanded(6.0f).reduced(0.5f), 10.0f, 1.0f);
    g.setColour(kDark); g.fillRoundedRectangle(wp, 6.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x12ffffff), 0, wp.getY(), juce::Colour(0x00ffffff), 0, wp.getY() + wp.getHeight() * 0.5f, false));
    g.fillRoundedRectangle(wp, 6.0f);
    g.setColour(juce::Colour(0xff25262a)); g.drawRoundedRectangle(wp.reduced(0.5f), 6.0f, 1.0f);
    if (stage == nfvs::Fix || stage == nfvs::Tone) drawSpectrum(g, wp, accent, modOn);
    else if (stage == nfvs::Eq) drawEq(g, wp, accent, modOn);
    else if (stage == nfvs::Voices) drawVoices(g, wp, accent, modOn);
    else if (stage == nfvs::Harmony) drawHarmony(g, wp, accent, modOn);
    else
    {
        const int pos = chain.histWritePos(), cols = nfvs::Chain::kHist;
        const float laneH = (wp.getHeight() - 24.0f) * 0.5f;
        for (int lane = 0; lane < 2; ++lane)
        {
            const auto lr = juce::Rectangle<float>(wp.getX() + 10.0f, wp.getY() + 8.0f + (laneH + 8.0f) * (float) lane, wp.getWidth() - 20.0f, laneH);
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
        g.setColour(accent); g.drawText(juce::String("AFTER ") + si.name, juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 16.0f + laneH, 200.0f, 12.0f), juce::Justification::centredLeft);
    }
    if (stage == nfvs::Control)
    {
        const float gr = chain.infoAt(nfvs::Control);
        const auto br = juce::Rectangle<float>(wp.getRight() - 190.0f, wp.getY() + 14.0f, 170.0f, 8.0f);
        g.setColour(kDark.withAlpha(0.92f)); g.fillRoundedRectangle(br.expanded(10.0f, 8.0f).withHeight(44.0f).translated(0.0f, -2.0f), 6.0f);
        g.setColour(kMuted); g.setFont(font(9.0f, true));
        g.drawText("GAIN REDUCTION", juce::Rectangle<float>(br.getX(), br.getY() + 10.0f, 110.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour(kText); g.drawText("-" + juce::String(gr, 1) + " dB", juce::Rectangle<float>(br.getRight() - 60.0f, br.getY() + 10.0f, 60.0f, 12.0f), juce::Justification::centredRight);
        g.setColour(juce::Colour(0xff1d1d21)); g.fillRoundedRectangle(br, 4.0f);
        g.setColour(modOn ? accent : juce::Colour(0xff55575e)); g.fillRoundedRectangle(br.withWidth(std::max(4.0f, br.getWidth() * juce::jlimit(0.0f, 1.0f, gr / 18.0f))), 4.0f);
    }

    // ---- STYLE: segmented selector
    recess(juce::Rectangle<float>(180.0f, 224.0f, 366.0f, 34.0f), 7.0f);
    const int mode = juce::jlimit(0, 2, juce::roundToInt(pv(sid("mode"))));
    for (int i = 0; i < 3; ++i)
    {
        const auto r = modeRect(i);
        const bool sel = i == mode && stage != nfvs::Eq;   // EQ styles are one-shot curve buttons
        if (sel)
        {
            g.setGradientFill(juce::ColourGradient(accent.brighter(0.25f), 0, r.getY(), accent.darker(0.35f), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(juce::Colour(0x55ffffff)); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
        }
        g.setColour(sel ? juce::Colour(0xff0f1012) : kMuted); g.setFont(font(11.0f, true));
        g.drawText(si.modes[i], r, juce::Justification::centred);
    }

    // ---- options A / B: push buttons with an LED
    {
        const bool states[3] = { false, pv(sid("a")) > 0.5f, pv(sid("b")) > 0.5f };
        const juce::String labels[3] = { "", si.toggleA, si.toggleB };
        for (int row = 1; row <= 2; ++row)
        {
            const auto r = switchRow(row);
            const bool on = states[row] && modOn;
            g.setGradientFill(juce::ColourGradient(juce::Colour(on ? 0xff45484f : 0xff2e3035), 0, r.getY(), juce::Colour(on ? 0xff2c2e33 : 0xff1c1d20), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(juce::Colour(0xaa000000)); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.2f);
            g.setColour(juce::Colour(0x30ffffff)); g.drawLine(r.getX() + 6.0f, r.getY() + 1.5f, r.getRight() - 6.0f, r.getY() + 1.5f, 1.0f);
            if (states[row]) glow(r.getX() + 14.0f, r.getCentreY(), 3.6f, modOn ? accent : juce::Colour(0xff55575e));
            else { g.setColour(juce::Colour(0xff0e0f10)); g.fillEllipse(r.getX() + 10.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f); }
            g.setColour(on ? kText : kMuted); g.setFont(font(10.0f, true));
            g.drawText(labels[row], r.withTrimmedLeft(26.0f), juce::Justification::centredLeft);
        }
    }

    // ---- stage name
    {
        juce::String spaced; for (auto ch : juce::String(si.name)) spaced << ch << " ";
        g.setColour(modOn ? accent : juce::Colour(0xff6b6f77)); g.setFont(font(13.0f, true));
        g.drawText(spaced.trim(), juce::Rectangle<float>(182.0f, 262.0f, 300.0f, 18.0f), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff595c63)); g.setFont(font(8.5f));
        g.drawText(modOn ? "MODULE ON" : "MODULE OFF", juce::Rectangle<float>(W - 150.0f - 20.0f, 264.0f, 150.0f, 12.0f), juce::Justification::centredRight);
    }

    // ---- vertical LED meter (gain reduction / activity)
    {
        const auto mt = juce::Rectangle<float>(190.0f, 292.0f, 16.0f, 150.0f);
        recess(mt.expanded(5.0f, 5.0f), 4.0f);
        for (int j = 0; j < 15; ++j)
        {
            const bool lit = act * 15.0f > (float) j + 0.2f && modOn;
            const bool hot = si.activity[0] != 0;   // amber / red only where the meter shows real reduction
            const auto col = hot && j >= 13 ? juce::Colour(0xffe8532a) : hot && j >= 10 ? juce::Colour(0xffe8b23a) : accent;
            g.setColour(lit ? col : juce::Colour(0xff1b1c1f));
            g.fillRoundedRectangle(mt.getX(), mt.getBottom() - (float) (j + 1) * 10.0f + 1.0f, mt.getWidth(), 8.0f, 1.5f);
        }
        static const char* kMeter[nfvs::kStages] = { "NR", "CUT", "EQ", "TONE", "GR", "COL", "VOX", "HARM", "REV", "DLY", "LIM" };
        g.setColour(kMuted); g.setFont(font(8.0f, true));
        g.drawText(si.activity[0] ? kMeter[stage] : "AMT", juce::Rectangle<float>(mt.getX() - 12.0f, mt.getBottom() + 8.0f, 40.0f, 10.0f), juce::Justification::centred);
    }

    // ---- LCD value window next to the big knob
    {
        const float amtV = pv(sid("amt"));
        const auto lcd = juce::Rectangle<float>(440.0f, 338.0f, 100.0f, 60.0f);
        g.setColour(juce::Colour(0xff0a120d)); g.fillRoundedRectangle(lcd, 6.0f);
        g.setColour(juce::Colour(0xaa000000)); g.drawRoundedRectangle(lcd.reduced(0.5f), 6.0f, 1.2f);
        g.setColour(juce::Colour(0x1cffffff)); g.drawLine(lcd.getX() + 6.0f, lcd.getBottom() + 0.5f, lcd.getRight() - 6.0f, lcd.getBottom() + 0.5f, 1.0f);
        const auto col = modOn ? accent : juce::Colour(0xff55575e);
        g.setColour(col); g.setFont(font(24.0f, true));
        g.drawText(juce::String(juce::roundToInt(amtV)) + " %", lcd.withTrimmedBottom(stage == nfvs::Control ? 14.0f : 8.0f), juce::Justification::centred);
        g.setColour(kMuted); g.setFont(font(8.5f, true));
        g.drawText(stage == nfvs::Control ? "THR " + juce::String(juce::roundToInt(-9.0f - 22.0f * amtV / 100.0f)) + " dB" : juce::String("AMOUNT"),
                   juce::Rectangle<float>(lcd.getX(), lcd.getBottom() - 17.0f, lcd.getWidth(), 12.0f), juce::Justification::centred);
    }

    // ---- right area: compressor trims, harmony key, or what the module does
    if (stage == nfvs::Control)
    {
        const std::pair<juce::Slider*, const char*> kn[] = { { &atkKnob, "ATTACK" }, { &relKnob, "RELEASE" }, { &ratKnob, "RATIO" } };
        for (const auto& k : kn)
        {
            const float cx = (float) k.first->getBounds().getCentreX() / scale();
            g.setColour(kText); g.setFont(font(10.0f, true));
            g.drawText(k.first->getTextFromValue(k.first->getValue()), juce::Rectangle<float>(cx - 36.0f, 282.0f, 72.0f, 14.0f), juce::Justification::centred);
            g.setColour(kMuted); g.setFont(font(8.5f, true));
            g.drawText(k.second, juce::Rectangle<float>(cx - 30.0f, 338.0f, 60.0f, 12.0f), juce::Justification::centred);
        }
    }
    else if (stage == nfvs::Harmony)
    {
        static const char* kKeys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto kr = keyRect();
        recess(kr, 6.0f);
        g.setColour(kMuted); g.setFont(font(9.5f, true)); g.drawText("KEY", kr.withTrimmedLeft(16.0f), juce::Justification::centredLeft);
        g.setColour(accent); g.setFont(font(16.0f, true));
        g.drawText(kKeys[juce::jlimit(0, 11, juce::roundToInt(pv(keyId())))], kr.withTrimmedRight(16.0f), juce::Justification::centredRight);
    }
    else
    {
        const auto card = juce::Rectangle<float>(552.0f, 280.0f, 240.0f, 68.0f);
        recess(card, 6.0f);
        g.setColour(juce::Colour(0xff8f939b)); g.setFont(font(10.5f));
        g.drawFittedText(si.hint, card.reduced(12.0f, 8.0f).toNearestInt(), juce::Justification::centredLeft, 3);
    }

    // ---- master strip: low cut, high cut, output, mix
    recess(juce::Rectangle<float>(546.0f, 366.0f, 246.0f, 100.0f), 8.0f);
    {
        const std::pair<juce::Slider*, const char*> kn[] = { { &lowKnob, "LOW CUT" }, { &highKnob, "HIGH CUT" }, { &outKnob, "OUTPUT" }, { &mixKnob, "MIX" } };
        for (const auto& k : kn)
        {
            const float cx = (float) k.first->getBounds().getCentreX() / scale();
            g.setColour(kText); g.setFont(font(9.5f, true));
            g.drawText(k.first->getTextFromValue(k.first->getValue()), juce::Rectangle<float>(cx - 30.0f, 372.0f, 60.0f, 13.0f), juce::Justification::centred);
            g.setColour(kMuted); g.setFont(font(8.5f, true));
            g.drawText(k.second, juce::Rectangle<float>(cx - 30.0f, 442.0f, 60.0f, 12.0f), juce::Justification::centred);
        }
    }

    g.setColour(juce::Colour(0xff6b6f77)); g.setFont(font(9.0f));
    g.drawText("V" JucePlugin_VersionString, juce::Rectangle<float>(W * 0.5f - 30.0f, H - 19.0f, 60.0f, 12.0f), juce::Justification::centred);
    g.drawText("By Nenno Fernando", juce::Rectangle<float>(W - 30.0f - 160.0f, H - 19.0f, 160.0f, 12.0f), juce::Justification::centredRight);
}

// HARMONY display: the lead in the middle of a row of interval voices; click a voice to turn it on or off.
void NFVocalSuiteAudioProcessorEditor::drawHarmony(juce::Graphics& g, juce::Rectangle<float> wp, juce::Colour accent, bool modOn)
{
    const auto& chain = proc.chainRef();
    const int pos = chain.histWritePos(), cols = nfvs::Chain::kHist;
    float lvl = 0.0f;
    for (int k = 1; k <= 6; ++k) lvl = std::max(lvl, chain.histAt(0, (pos - k + cols) % cols));
    const float n = juce::jlimit(0.0f, 1.0f, (nfvs::toDb(lvl) + 54.0f) / 54.0f);
    const float amt = modOn ? pv(sid("amt")) / 100.0f : 0.0f;
    static const char* kNames[7] = { "-8VE", "-3RD", "LEAD", "+3RD", "+5TH", "+6TH", "+8VE" };
    static const int kColToHv[7] = { 0, 1, -1, 2, 3, 4, 5 };
    static const char* kNote[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float base = wp.getBottom() - 46.0f, full = wp.getHeight() - 82.0f;
    g.setColour(juce::Colour(0xff222226)); g.drawHorizontalLine((int) base, wp.getX() + 14.0f, wp.getRight() - 14.0f);
    for (int c = 0; c < 7; ++c)
    {
        const auto chip = harmChip(c);
        const bool lead = kColToHv[c] < 0;
        const bool on = lead ? true : pv("hv" + juce::String(kColToHv[c] + 1)) > 0.5f;
        const float x = chip.getCentreX();
        const float h = lead ? std::max(3.0f, full * n) : (on ? std::max(3.0f, full * n * (0.1f + 0.9f * amt) * 0.8f) : 2.0f);
        const auto col = lead ? juce::Colour(0xffe9ebef) : (on && modOn ? accent : juce::Colour(0xff4a4c52));
        if (on) { g.setColour(col.withAlpha(0.20f)); g.fillEllipse(x - 9.0f, base - h - 9.0f, 18.0f, 18.0f); }
        g.setColour(col); g.drawLine(x, base, x, base - h, lead ? 2.4f : 2.0f);
        g.fillEllipse(x - 3.5f, base - h - 3.5f, 7.0f, 7.0f);
        g.setColour(on && !lead ? accent.withAlpha(0.16f) : juce::Colour(0xff131316)); g.fillRoundedRectangle(chip, 13.0f);
        g.setColour(lead ? kLine : (on ? accent : kLine)); g.drawRoundedRectangle(chip.reduced(0.5f), 13.0f, 1.0f);
        g.setFont(font(10.5f, true));
        if (lead)
        {
            const float m = chain.leadMidiNow();
            g.setColour(kText);
            g.drawText(m < 0.0f ? juce::String("LEAD") : "LEAD " + juce::String(kNote[((int) std::lround(m) % 12 + 12) % 12]) + juce::String((int) std::lround(m) / 12 - 1), chip, juce::Justification::centred);
        }
        else { g.setColour(on ? kText : kMuted); g.drawText(kNames[c], chip, juce::Justification::centred); }
    }
    g.setFont(font(9.0f, true)); g.setColour(accent);
    g.drawText("HARMONY", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 6.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour(kMuted); g.drawText("Click a voice to turn it on or off. MAJOR / MINOR follow your key; FIXED uses plain semitones.",
                                    juce::Rectangle<float>(wp.getX() + 14.0f, wp.getY() + 20.0f, 520.0f, 12.0f), juce::Justification::centredLeft);
}

// ---- EQ -------------------------------------------------------------------------------------------------------------
juce::Point<float> NFVocalSuiteAudioProcessorEditor::eqNodePos(int b) const
{
    const auto r = eqArea();
    const juce::String n = "eq" + juce::String(b + 1);
    const float f = pv(n + "f"), g = pv(n + "g");
    return { r.getX() + r.getWidth() * std::log(f / 30.0f) / std::log(600.0f), r.getCentreY() - g / 15.0f * (r.getHeight() * 0.5f - 6.0f) };
}

int NFVocalSuiteAudioProcessorEditor::eqNodeAt(juce::Point<float> pos) const
{
    int best = -1; float bd = 15.0f;
    for (int b = 0; b < 4; ++b) { const float d = eqNodePos(b).getDistanceFrom(pos); if (d < bd) { bd = d; best = b; } }
    return best;
}

void NFVocalSuiteAudioProcessorEditor::setEqFromPos(int b, juce::Point<float> pos)
{
    const auto r = eqArea();
    const juce::String n = "eq" + juce::String(b + 1);
    const float f = 30.0f * std::pow(600.0f, (pos.x - r.getX()) / r.getWidth());
    const float g = (r.getCentreY() - pos.y) / (r.getHeight() * 0.5f - 6.0f) * 15.0f;
    if (auto* pf = proc.apvts.getParameter(n + "f")) pf->setValueNotifyingHost(pf->convertTo0to1(f));
    if (auto* pg = proc.apvts.getParameter(n + "g")) pg->setValueNotifyingHost(pg->convertTo0to1(juce::jlimit(-15.0f, 15.0f, std::round(g * 10.0f) / 10.0f)));
}

// The EQ styles are starting curves: they set the four points, which can then be dragged.
void NFVocalSuiteAudioProcessorEditor::applyEqCurve(int style)
{
    //                    f1     g1    f2     g2    q2    f3      g3    q3    f4      g4
    static const float k[3][10] = { {  120.0f, -3.0f, 300.0f, -3.0f, 1.2f, 3000.0f,  1.5f, 1.0f, 10000.0f,  2.0f },     // CLEAN
                                    {  120.0f, -2.0f, 400.0f, -2.0f, 1.0f, 3500.0f,  3.0f, 1.0f, 11000.0f,  4.0f },     // BRIGHT
                                    {  150.0f,  3.0f, 250.0f,  1.5f, 0.9f, 4000.0f, -2.0f, 0.9f,  9000.0f, -1.5f } };   // WARM
    const float* v = k[juce::jlimit(0, 2, style)];
    auto set = [&](const juce::String& id, float value) {
        if (auto* prm = proc.apvts.getParameter(id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->convertTo0to1(value)); prm->endChangeGesture(); }
    };
    set("eq1f", v[0]); set("eq1g", v[1]); set("eq2f", v[2]); set("eq2g", v[3]); set("eq2q", v[4]);
    set("eq3f", v[5]); set("eq3g", v[6]); set("eq3q", v[7]); set("eq4f", v[8]); set("eq4g", v[9]);
}

static float biquadDb(const nfvs::Biquad& q, float w)
{
    const std::complex<float> z1 = std::polar(1.0f, -w), z2 = z1 * z1;
    const std::complex<float> num = q.b0 + q.b1 * z1 + q.b2 * z2, den = 1.0f + q.a1 * z1 + q.a2 * z2;
    return 20.0f * std::log10(std::abs(num / den) + 1e-9f);
}

void NFVocalSuiteAudioProcessorEditor::drawEq(juce::Graphics& g, juce::Rectangle<float> wp, juce::Colour accent, bool modOn)
{
    drawSpectrum(g, wp, accent, modOn);   // the signal before / after, in the background
    const auto r = eqArea();
    auto yOfG = [&](float db) { return r.getCentreY() - db / 15.0f * (r.getHeight() * 0.5f - 6.0f); };
    g.setFont(font(8.5f));
    for (float db : { 12.0f, 0.0f, -12.0f })
    {
        g.setColour(juce::Colour(db == 0.0f ? 0x40ffffff : 0x18ffffff)); g.drawHorizontalLine((int) yOfG(db), r.getX(), r.getRight() - 28.0f);
        g.setColour(kMuted); g.drawText((db > 0 ? "+" : "") + juce::String((int) db), juce::Rectangle<float>(r.getRight() - 26.0f, yOfG(db) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }
    // effective response (what the signal really gets: node gains x amount x switches)
    const float amt = modOn ? pv(sid("amt")) / 100.0f : 0.0f;
    const bool sw[4] = { pv(sid("a")) > 0.5f, pv(sid("b")) > 0.5f, pv(sid("b")) > 0.5f, pv(sid("a")) > 0.5f };
    nfvs::Biquad bq[4];
    auto bn = [&](int b) { return "eq" + juce::String(b + 1); };
    auto gain = [&](int b) { return pv(bn(b) + "g") * amt * (sw[b] ? 1.0f : 0.0f); };
    bq[0].lowShelf(48000.0, pv(bn(0) + "f"), gain(0));
    bq[1].peak(48000.0, pv(bn(1) + "f"), pv(bn(1) + "q"), gain(1));
    bq[2].peak(48000.0, pv(bn(2) + "f"), pv(bn(2) + "q"), gain(2));
    bq[3].highShelf(48000.0, pv(bn(3) + "f"), gain(3));
    juce::Path curve;
    for (int i = 0; i < 200; ++i)
    {
        const float f = 30.0f * std::pow(600.0f, (float) i / 199.0f), w = 2.0f * juce::MathConstants<float>::pi * f / 48000.0f;
        float db = 0.0f; for (auto& q : bq) db += biquadDb(q, w);
        const float x = r.getX() + r.getWidth() * (float) i / 199.0f, y = yOfG(juce::jlimit(-16.0f, 16.0f, db));
        if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xffe9ecf2).withAlpha(modOn ? 1.0f : 0.4f)); g.strokePath(curve, juce::PathStrokeType(2.2f));
    // points
    for (int b = 0; b < 4; ++b)
    {
        const auto p = eqNodePos(b);
        const bool on = modOn && sw[b], sel = b == selBand;
        g.setColour(juce::Colour(on ? 0xffe9ecf2 : 0xff55575e).withAlpha(0.18f)); g.fillEllipse(p.x - 11.0f, p.y - 11.0f, 22.0f, 22.0f);
        g.setColour(juce::Colour(on ? 0xffe9ecf2 : 0xff55575e)); g.fillEllipse(p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        if (sel) { g.setColour(juce::Colours::white); g.drawEllipse(p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f, 1.4f); }
        g.setColour(juce::Colour(0xff15151a)); g.setFont(font(9.0f, true)); g.drawText(juce::String(b + 1), juce::Rectangle<float>(p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f), juce::Justification::centred);
    }
    const juce::String n = bn(selBand);
    juce::String txt = "BAND " + juce::String(selBand + 1) + "   " + juce::String(juce::roundToInt(pv(n + "f"))) + " Hz   " + (pv(n + "g") > 0.04f ? "+" : "") + juce::String(pv(n + "g"), 1) + " dB";
    if (selBand == 1 || selBand == 2) txt += "   Q " + juce::String(pv(n + "q"), 2);
    g.setColour(kText); g.setFont(font(10.5f, true));
    g.drawText(txt, juce::Rectangle<float>(wp.getRight() - 330.0f, wp.getY() + 6.0f, 316.0f, 14.0f), juce::Justification::centredRight);
    g.setColour(kMuted); g.setFont(font(9.0f));
    g.drawText("Drag the points. Mouse wheel on a bell = width (Q). Double-click = reset.", juce::Rectangle<float>(wp.getX() + 14.0f, wp.getBottom() - 20.0f, 420.0f, 12.0f), juce::Justification::centredLeft);
}
