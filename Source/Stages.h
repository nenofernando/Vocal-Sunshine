#pragma once
#include <JuceHeader.h>

// Static description of the 7 stages (names, styles, option labels, defaults, colours). Shared by processor and editor.
namespace nfvs
{
constexpr int kStages = 7;

struct StageInfo
{
    const char* name;
    const char* modes[3];
    const char* toggleA;
    const char* toggleB;
    float defAmt;
    bool defA, defB;
    juce::uint32 colour;
    const char* hint;
    const char* activity;   // label of the activity meter ("" = shows the amount)
};

inline const StageInfo kStageInfo[kStages] = {
    { "CLEAN",   { "VOICE", "ROOM", "NOISY" },          "LOW CUT",    "DENOISE",     30.0f, true,  true,  0xff4cd07a, "Removes rumble and background noise",        "NOISE REDUCTION" },
    { "FIX",     { "SOFT", "MEDIUM", "STRONG" },        "DE-ESS",     "RESONANCES",  40.0f, true,  true,  0xff35c7c0, "Tames harsh S sounds and boxy or nasal peaks", "CUT" },
    { "TONE",    { "WARM", "BALANCED", "BRIGHT" },      "AIR",        "BODY",        40.0f, true,  true,  0xffe8863a, "Shapes warmth, clarity and air",               "" },
    { "CONTROL", { "GENTLE", "PUNCHY", "HEAVY" },       "AUTO GAIN",  "LEVEL RIDER", 50.0f, true,  true,  0xff4a86ff, "Evens out the level of the voice",             "GAIN REDUCTION" },
    { "COLOR",   { "TAPE", "TUBE", "EXCITER" },         "SPARKLE",    "LOW THUMP",   20.0f, false, false, 0xffe5566d, "Adds harmonics and character",                 "" },
    { "SPACE",   { "PLATE", "ROOM", "HALL" },           "DELAY",      "DUCK",        25.0f, false, true,  0xff8a5cf0, "Reverb and echo that stay out of the way",     "" },
    { "FINISH",  { "SOFT", "MEDIUM", "LOUD" },          "LIMITER",    "CLIP",        40.0f, true,  false, 0xffc8e63a, "Final loudness with peak safety",              "LIMITER" },
};
} // namespace nfvs
