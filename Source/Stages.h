#pragma once
#include <JuceHeader.h>

// Static description of the 12 stages (names, styles, option labels, defaults, colours). Shared by processor and editor.
namespace nfvs
{
constexpr int kStages = 12;
enum Stage { Clean = 0, Fix, Eq, Tone, Control, DeEss, Color, Voices, Harmony, Reverb, Delay, Finish };

struct StageInfo
{
    const char* name;
    const char* modes[3];
    const char* toggleA;
    const char* toggleB;
    float defAmt;
    int defMode;
    bool defA, defB;
    juce::uint32 colour;
    const char* hint;
    const char* activity;   // label of the activity meter ("" = shows the amount)
};

inline const StageInfo kStageInfo[kStages] = {
    { "CLEAN",   { "VOICE", "ROOM", "NOISY" },       "AUTO LOW CUT", "DENOISE",     30.0f, 0, true,  true,  0xff39ff14, "Removes rumble and background noise",           "NOISE REDUCTION" },
    { "FIX",     { "SOFT", "MEDIUM", "STRONG" },     "BOXY / NASAL", "HARSH",       40.0f, 1, true,  true,  0xff00ffc8, "Tames boxy, nasal and harsh peaks",              "CUT" },
    { "EQ",      { "CLEAN", "BRIGHT", "WARM" },     "SHELVES",      "BELLS",      100.0f, 0, true,  true,  0xffff2ec4, "Drag the 4 points on the graph. Mouse wheel on a bell = width (Q). Double-click a point = reset.", "" },
    { "TONE",    { "WARM", "BALANCED", "BRIGHT" },   "AIR",          "BODY",        40.0f, 1, true,  true,  0xffff8a00, "Shapes warmth, clarity and air",                "" },
    { "COMPRESSOR", { "GENTLE", "PUNCHY", "HEAVY" }, "AUTO GAIN",   "LEVEL RIDER", 50.0f, 1, true,  true,  0xff2b8cff, "Compressor and level rider to even out the voice", "GAIN REDUCTION" },
    { "DE-ESSER", { "NARROW", "MEDIUM", "WIDE" },   "LISTEN",       "SMOOTH",      40.0f, 1, false, true,  0xff00e5ff, "Softens harsh S and SH sounds at the frequency you choose", "REDUCTION" },
    { "COLOR",   { "TAPE", "TUBE", "EXCITER" },      "SPARKLE",      "LOW THUMP",   20.0f, 0, false, false, 0xffff1f4b, "Adds harmonics and character",                  "" },
    { "VOICES",  { "DOUBLE", "STACK", "WIDE" },      "LOW MONO",     "SOFTEN",      15.0f, 0, true,  false, 0xffd34dff, "Opens the voice into doubled, wide copies",     "" },
    { "HARMONY", { "MAJOR", "MINOR", "FIXED" },  "SPREAD",      "SOFTEN",       0.0f, 0, true,  true,  0xffffe600, "Adds harmony voices at musical intervals",     "" },
    { "REVERB",  { "PLATE", "ROOM", "HALL" },        "BRIGHT",       "DUCK",        25.0f, 0, false, true,  0xff9d4dff, "Reverb that stays out of the way",              "" },
    { "DELAY",   { "1/8", "1/4", "1/4 DOTTED" },     "PING-PONG",    "DUCK",         0.0f, 1, true,  true,  0xff5a6bff, "Echo locked to the project tempo",              "" },
    { "FINISH",  { "SOFT", "MEDIUM", "LOUD" },       "LIMITER",      "CLIP",        40.0f, 1, true,  false, 0xffc8ff00, "Final loudness with peak safety",               "LIMITER" },
};
} // namespace nfvs
