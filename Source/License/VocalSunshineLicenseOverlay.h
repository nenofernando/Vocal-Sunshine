#pragma once
// Activation window shown over the NF Vocal Sunshine editor until a valid licence key is on this computer.
#include <juce_gui_basics/juce_gui_basics.h>
#include "NFVSLicense.h"

class VocalSunshineLicenseOverlay final : public juce::Component, private juce::Timer
{
public:
    std::function<bool()> isLicensed;          // asked about twice a second: the overlay hides itself when another instance activated
    std::function<void()> onActivated;
    VocalSunshineLicenseOverlay()
    {
        using namespace juce;
        keyBox.setMultiLine(false);
        keyBox.setJustification(Justification::centred);
        keyBox.setFont(FontOptions(16.0f, Font::bold));
        keyBox.setTextToShowWhenEmpty("NFVS-XXXX-XXXX-XXXX-XXXX", Colour(0xff6b6f77));
        keyBox.setColour(TextEditor::backgroundColourId, Colour(0xff0b0c0f)); keyBox.setColour(TextEditor::textColourId, Colours::white);
        keyBox.setColour(TextEditor::outlineColourId, Colour(0xff4a4200)); keyBox.setColour(TextEditor::focusedOutlineColourId, accent);
        keyBox.onReturnKey = [this] { activate(); }; addAndMakeVisible(keyBox);
        button.setColour(TextButton::buttonColourId, accent); button.setColour(TextButton::textColourOffId, Colour(0xff15130a));
        button.onClick = [this] { activate(); }; addAndMakeVisible(button);
        startTimerHz(2);
    }
    void resized() override
    {
        auto c = getLocalBounds().getCentre();
        keyBox.setBounds(juce::Rectangle<int>(juce::jmax(280, getWidth() / 2), 34).withCentre({ c.x, c.y + 6 }));
        button.setBounds(juce::Rectangle<int>(150, 32).withCentre({ c.x, c.y + 56 }));
    }
    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll(Colour(0xf0101114)); auto c = getLocalBounds().getCentre();
        g.setColour(accent); g.setFont(FontOptions(20.0f, Font::bold));
        g.drawText("ATIVAR O NF VOCAL SUNSHINE  /  ACTIVATE", getLocalBounds().withHeight(30).withCentre({ c.x, c.y - 62 }), Justification::centred);
        g.setFont(FontOptions(12.5f)); g.setColour(Colour(0xffd8d9de));
        g.drawText("Digite a chave que voce recebeu apos a compra. / Type the key you received after purchase.", getLocalBounds().withHeight(20).withCentre({ c.x, c.y - 34 }), Justification::centred);
        g.setFont(FontOptions(11.0f)); g.setColour(Colour(0xff8b8f97));
        g.drawText("A primeira ativacao precisa de internet. / The first activation needs internet.", getLocalBounds().withHeight(18).withCentre({ c.x, c.y + 90 }), Justification::centred);
        g.setFont(FontOptions(12.5f, Font::bold)); g.setColour(message.isEmpty() ? Colours::transparentBlack : Colour(0xffff6f6f));
        g.drawText(message, getLocalBounds().withHeight(20).withCentre({ c.x, c.y + 112 }), Justification::centred);
    }
private:
    void timerCallback() override { if (isLicensed && isLicensed() && isVisible()) setVisible(false); else if (isLicensed && !isLicensed() && !isVisible()) { setVisible(true); toFront(false); } }
    void activate()
    {
        if (busy) return;
        const auto key = keyBox.getText().removeCharacters(" \t\r\n");
        if (key.isEmpty()) { message = "Digite a chave primeiro / Type the key first."; repaint(); return; }
        const bool shortCode = NFVSLicense::looksShort(key);
        if (!shortCode) { auto info = NFVSLicense::check(key); if (!info.valid) { message = info.message; repaint(); return; } }
        busy = true; message = "Verificando... / Checking..."; button.setEnabled(false); repaint();
        juce::Component::SafePointer<VocalSunshineLicenseOverlay> safe(this);
        juce::Thread::launch([safe, key]
        {
            const auto r = NFVSLicense::activateOnline(key);   // registers this computer (max 2 per licence); a short code comes back as the signed long key
            juce::MessageManager::callAsync([safe, key, r]
            {
                if (safe == nullptr) return;
                safe->busy = false; safe->button.setEnabled(true);
                if (r.state != NFVSLicense::Online::Ok) { safe->message = r.message; safe->repaint(); return; }
                const auto toStore = r.longKey.isNotEmpty() ? r.longKey : key;
                if (!NFVSLicense::check(toStore).valid) { safe->message = "The server answered with a key this plug-in does not accept."; safe->repaint(); return; }
                NFVSLicense::keyFile().getParentDirectory().createDirectory();
                NFVSLicense::keyFile().replaceWithText(toStore.removeCharacters(" \t\r\n"));
                NFVSLicense::writeStamp();
                if (safe->onActivated) safe->onActivated();
                safe->setVisible(false);
            });
        });
    }
    const juce::Colour accent { 0xffffd400 };
    juce::TextEditor keyBox; juce::TextButton button { "Ativar / Activate" }; juce::String message; bool busy = false;
};
