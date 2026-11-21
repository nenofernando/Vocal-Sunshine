#pragma once
#include <JuceHeader.h>

// Static description of the 10 stages (names, styles, option labels, defaults, colours). Shared by processor and editor.
namespace nfvs
{
constexpr int kStages = 10;
enum Stage { Clean = 0, Fix, Tone, Control, Color, Voices, Harmony, Reverb, Delay, Finish };

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
    { "CLEAN",   { "VOICE", "ROOM", "NOISY" },       "AUTO LOW CUT", "DENOISE",     30.0f, 0, true,  true,  0xff4cd07a, "Removes rumble and background noise",           "NOISE REDUCTION" },
    { "FIX",     { "SOFT", "MEDIUM", "STRONG" },     "DE-ESS",       "RESONANCES",  40.0f, 1, true,  true,  0xff35c7c0, "Tames harsh S sounds and boxy or nasal peaks", "CUT" },
    { "TONE",    { "WARM", "BALANCED", "BRIGHT" },   "AIR",          "BODY",        40.0f, 1, true,  true,  0xffe8863a, "Shapes warmth, clarity and air",                "" },
    { "COMPRESSOR", { "GENTLE", "PUNCHY", "HEAVY" }, "AUTO GAIN",   "LEVEL RIDER", 50.0f, 1, true,  true,  0xff4a86ff, "Compressor and level rider to even out the voice", "GAIN REDUCTION" },
    { "COLOR",   { "TAPE", "TUBE", "EXCITER" },      "SPARKLE",      "LOW THUMP",   20.0f, 0, false, false, 0xffe5566d, "Adds harmonics and character",                  "" },
    { "VOICES",  { "DOUBLE", "STACK", "WIDE" },      "LOW MONO",     "SOFTEN",      15.0f, 0, true,  false, 0xffd36bd8, "Opens the voice into doubled, wide copies",     "" },
    { "HARMONY", { "MAJOR", "MINOR", "FIXED" },  "SPREAD",      "SOFTEN",       0.0f, 0, true,  true,  0xffe3b341, "Adds harmony voices at musical intervals",     "" },
    { "REVERB",  { "PLATE", "ROOM", "HALL" },        "BRIGHT",       "DUCK",        25.0f, 0, false, true,  0xff8a5cf0, "Reverb that stays out of the way",              "" },
    { "DELAY",   { "1/8", "1/4", "1/4 DOTTED" },     "PING-PONG",    "DUCK",         0.0f, 1, true,  true,  0xff7f8cff, "Echo locked to the project tempo",              "" },
    { "FINISH",  { "SOFT", "MEDIUM", "LOUD" },       "LIMITER",      "CLIP",        40.0f, 1, true,  false, 0xffc8e63a, "Final loudness with peak safety",               "LIMITER" },
};
} // namespace nfvs
