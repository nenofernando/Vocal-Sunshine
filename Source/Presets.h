#pragma once

// Factory presets: amount (0..100), style (0..2) and flags per stage. flags: bit0 = module on, bit1 = option A, bit2 = option B.
// Not part of a preset: low cut / high cut / output / mix, the EQ points, the harmony intervals, the harmony and pitch keys, voices width, compressor trims.
struct VSPreset { const char* name; int amt[12]; int mode[12]; int flags[12]; };

//                  CLEAN PITCH FIX  EQ TONE COMP COLOR VOICE HARM REVERB DELAY FINISH
inline constexpr VSPreset kVSPresets[] = {
    { "Init",         { 30,  0, 40, 100, 40, 50, 20, 15,  0, 25,  0, 40 }, { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1 }, { 7, 5, 7, 7, 7, 7, 1, 3, 7, 5, 7, 3 } },
    { "Pop Female",   { 25,  0, 50, 100, 50, 55, 20, 30, 20, 35, 10, 50 }, { 0, 0, 1, 0, 2, 1, 0, 0, 0, 0, 1, 1 }, { 7, 5, 7, 7, 7, 7, 1, 3, 7, 5, 7, 3 } },
    { "Pop Male",     { 30,  0, 40, 100, 40, 55, 25, 15,  0, 30, 10, 50 }, { 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1 }, { 7, 5, 7, 7, 7, 7, 1, 3, 7, 5, 7, 3 } },
    { "Rap Close",    { 35,  0, 45, 100, 40, 70, 35, 10,  0, 15, 20, 65 }, { 1, 0, 1, 0, 1, 2, 0, 0, 0, 1, 1, 2 }, { 7, 5, 7, 7, 7, 7, 3, 3, 7, 5, 7, 3 } },
    { "Trap Vocal",   { 40, 70, 55, 100, 60, 70, 45, 45, 25, 45, 40, 70 }, { 1, 2, 1, 0, 2, 2, 2, 1, 2, 0, 1, 2 }, { 7, 3, 7, 7, 7, 7, 3, 3, 7, 7, 7, 7 } },
    { "Rock Vocal",   { 30,  0, 35, 100, 40, 65, 50, 25,  0, 25, 10, 60 }, { 1, 0, 1, 0, 1, 2, 1, 1, 0, 2, 1, 2 }, { 7, 5, 7, 7, 7, 7, 5, 3, 7, 5, 7, 7 } },
    { "Podcast",      { 55,  0, 50, 100, 35, 60,  5,  0,  0,  0,  0, 55 }, { 2, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1 }, { 7, 5, 7, 7, 7, 7, 0, 0, 0, 0, 0, 3 } },
    { "Warm Ballad",  { 20,  0, 30, 100, 65, 40, 35, 40, 25, 55, 20, 35 }, { 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0 }, { 7, 5, 7, 7, 5, 7, 1, 3, 7, 5, 7, 3 } },
    { "Bright Radio", { 25,  0, 50, 100, 70, 60, 30, 20, 15, 25, 10, 60 }, { 0, 0, 1, 0, 2, 1, 2, 0, 0, 0, 1, 1 }, { 7, 5, 7, 7, 7, 7, 3, 3, 7, 7, 7, 3 } },
    { "Raw Demo",     { 40,  0, 30, 100, 25, 30,  0,  0,  0, 10,  0, 30 }, { 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 }, { 7, 5, 7, 7, 7, 7, 0, 0, 0, 5, 0, 3 } },
    { "Live Room",    { 50,  0, 45, 100, 45, 45, 10,  0,  0,  5,  0, 40 }, { 2, 0, 2, 0, 1, 1, 0, 0, 0, 1, 1, 1 }, { 7, 5, 7, 7, 7, 7, 1, 0, 0, 5, 0, 3 } },
};
inline constexpr int kNumVSPresets = (int) (sizeof(kVSPresets) / sizeof(kVSPresets[0]));
