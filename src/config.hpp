#pragma once

#include <cstdint>
#include <string>

#include "mapping.hpp"

namespace ds2x {

// casual-player settings, read from settings.ini next to the exe.
// anything missing keeps its default ; CLI flags override the file.
struct Config {
    int hz = 250;             // poll rate
    float deadzone = 0.08f;   // stick deadzone, 0..1
    float anti_deadzone = 0.0f;  // DS4Windows-style output floor past the deadzone, 0..1
    float max_zone = 1.0f;       // physical deflection that means "full" (worn sticks), <=1
    bool gyro = false;        // gyro steers the right stick
    float gyro_gain = 0.6f;   // how hard the gyro pushes the stick
    bool gyro_invert_yaw = false;    // flip left/right gyro direction
    bool gyro_invert_pitch = false;  // flip up/down gyro direction
    bool lightbar = false;    // set a light bar color at startup
    int player_led = 0;       // DualSense player LED 1..5, 0 = off
    int trigger_mode = 0;     // DualSense adaptive triggers: 0 off, 1 feedback, 2 weapon, 3 vibration
    int trigger_start = 2;    // zone where the effect starts (0-9)
    int trigger_strength = 6; // 1-8
    int trigger_freq = 40;    // vibration frequency in Hz
    std::uint8_t lr = 0, lg = 80, lb = 255;  // the color, r/g/b
    bool safe_mode = false;   // true = never write anything to the pad (read-only)
    Remap remap = default_remap();  // button remapping from remap_* keys
    std::string source_file;  // empty = auto
};

// parse a tiny ini file: [section] ignored, key=value, # or ; comments.
// returns false if the file doesn't exist / can't be opened (defaults stay).
// typos are reported with log_warn (unknown key, unparsable value) instead of
// being silently ignored.
bool load_config(const std::string& path, Config& cfg);

// clamp every setting into a range the rest of the program can safely use
// (e.g. hz 0 would divide by zero, deadzone 1.0 would flatten the sticks).
// returns the number of values that had to be changed ; each one is logged.
int sanitize_config(Config& cfg);

} // namespace ds2x
