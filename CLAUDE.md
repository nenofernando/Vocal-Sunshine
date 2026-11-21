# NF Vocal Suite
Guided seven-stage vocal processor (own design; the idea came from a "vocal chain in minutes" wizard seen in a video — never copy another product's name, art or presets).
Signal chain, zero latency: CLEAN -> FIX -> TONE -> CONTROL -> COLOR -> SPACE -> FINISH, then Output + Mix. One stage is shown at a time:
big amount knob, 3 styles, module on/off, 2 option switches, before/after waveform (input vs the signal after the selected stage), activity meter, stage rail at the bottom.
- Repo `~/NF-Vocal-Suite` (local git, not pushed). Version: `CMakeLists.txt` line 2 only (no installer yet). Licence OFF while developing (Neno adds it at the end with Paulo).
- Ids: bundle `com.nfaudiotools.nfvocalsuite`, manufacturer `Nfat`, code `Nfvs`, licence code would be `NF_VOCAL_SUITE`.
- Window 810 x 500 (exception to 810 x 270), size stored in state (`uiWidth`), selected stage in state (`stage`); logo double-click resets. Official logo image only.
- Preset tab 157 x 21 top right `[<] name [>]`, 11 factory presets (`Presets.h`), name in state (`presetName`).
- Stage names/styles/options/colours/defaults: `Source/Stages.h` (single table for DSP defaults and UI). DSP: `Source/SuiteDSP.h`. Params per stage: `s<N>amt|mode|on|a|b` (N = 1..7) + `output`, `mix`, `delayDiv`, `bypass`.
- DSP notes: FIX resonances are 3 self-calibrating subtractive band cuts (320 / 1000 / 3400 Hz, relative to each band's own running average); CONTROL = level rider (slow) -> linked compressor; COLOR extra terms are DC-blocked so the stage is transparent at 0; FINISH = drive -> soft clip -> peak limiter at -1 dB (attack is fast but not look-ahead, final soft clip catches overshoots).
- Build (Mac is Intel): `cmake -B build -DCMAKE_OSX_ARCHITECTURES=x86_64 -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/JUCE -DNFVocalSuite_BUILD_TOOLS=ON`; `NFVocalSuiteSnapshot out.png [stage 0..6] [scale] [preset]` renders the UI; `NFVocalSuiteSmokeTest` checks every preset and each stage's DSP behaviour.
