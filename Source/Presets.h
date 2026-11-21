#pragma once

// Factory presets: amount (0..100), style (0..2) and flags per stage. flags: bit0 = module on, bit1 = option A, bit2 = option B.
// Low cut / high cut / output / mix are not part of a preset.
struct VSPreset { const char* name; int amt[9]; int mode[9]; int flags[9]; };

//                  CLEAN FIX TONE CTRL COLOR VOICE REVERB DELAY FINISH
inline constexpr VSPreset kVSPresets[] = {
    { "Init",         { 30, 40, 40, 50, 20, 15, 25,  0, 40 }, { 0, 1, 1, 1, 0, 0, 0, 1, 1 }, { 7, 7, 7, 7, 1, 3, 5, 7, 3 } },
    { "Pop Female",   { 25, 50, 50, 55, 20, 30, 35, 10, 50 }, { 0, 1, 2, 1, 0, 0, 0, 1, 1 }, { 7, 7, 7, 7, 1, 3, 5, 7, 3 } },
    { "Pop Male",     { 30, 40, 40, 55, 25, 15, 30, 10, 50 }, { 0, 1, 0, 1, 1, 0, 0, 1, 1 }, { 7, 7, 7, 7, 1, 3, 5, 7, 3 } },
    { "Rap Close",    { 35, 45, 40, 70, 35, 10, 15, 20, 65 }, { 1, 1, 1, 2, 0, 0, 1, 1, 2 }, { 7, 7, 7, 7, 3, 3, 5, 7, 3 } },
    { "Trap Vocal",   { 40, 55, 60, 70, 45, 45, 45, 40, 70 }, { 1, 1, 2, 2, 2, 1, 0, 1, 2 }, { 7, 7, 7, 7, 3, 3, 7, 7, 7 } },
    { "Rock Vocal",   { 30, 35, 40, 65, 50, 25, 25, 10, 60 }, { 1, 1, 1, 2, 1, 1, 2, 1, 2 }, { 7, 7, 7, 7, 5, 3, 5, 7, 7 } },
    { "Podcast",      { 55, 50, 35, 60,  5,  0,  0,  0, 55 }, { 2, 1, 0, 1, 0, 0, 0, 1, 1 }, { 7, 7, 7, 7, 0, 0, 0, 0, 3 } },
    { "Warm Ballad",  { 20, 30, 65, 40, 35, 40, 55, 20, 35 }, { 0, 0, 0, 0, 1, 0, 2, 0, 0 }, { 7, 7, 5, 7, 1, 3, 5, 7, 3 } },
    { "Bright Radio", { 25, 50, 70, 60, 30, 20, 25, 10, 60 }, { 0, 1, 2, 1, 2, 0, 0, 1, 1 }, { 7, 7, 7, 7, 3, 3, 7, 7, 3 } },
    { "Raw Demo",     { 40, 30, 25, 30,  0,  0, 10,  0, 30 }, { 2, 0, 0, 0, 0, 0, 0, 1, 0 }, { 7, 7, 7, 7, 0, 0, 5, 0, 3 } },
    { "Live Room",    { 50, 45, 45, 45, 10,  0,  5,  0, 40 }, { 2, 2, 1, 1, 0, 0, 1, 1, 1 }, { 7, 7, 7, 7, 1, 0, 5, 0, 3 } },
};
inline constexpr int kNumVSPresets = (int) (sizeof(kVSPresets) / sizeof(kVSPresets[0]));
