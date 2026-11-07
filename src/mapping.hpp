#pragma once

#include "hid_report.hpp"

#include <array>
#include <string>

namespace ds2x {

 // Xbox 360 (XInput) button bits ; matches the wButtons bitmask games expect.
enum XboxButton : std::uint16_t {
    XDpadUp    = 0x0001,
    XDpadDown  = 0x0002,
    XDpadLeft  = 0x0004,
    XDpadRight = 0x0008,
    XStart     = 0x0010,
    XBack      = 0x0020,
    XLS        = 0x0040,  // left thumb click
    XRS        = 0x0080,
    XLB        = 0x0100,
    XRB        = 0x0200,
    XGuide     = 0x0400,
    XA         = 0x1000,
    XB         = 0x2000,
    XX         = 0x4000,
    XY         = 0x8000,
};

// remap table: for each of the 14 PlayStation button bits, which Xbox button
// bit(s) it should produce. zero means "drop it" (e.g. touchpad click by default).
using Remap = std::array<std::uint16_t, 14>;

Remap default_remap();
// parse one "remap_<ps>=<xbox>" line ; returns false if either name is unknown
bool parse_remap(const std::string& ps_name, const std::string& xbox_name, Remap& out);
int ps_button_index(const std::string& name);   // -1 if unknown
int xbox_button_bit(const std::string& name);   // 0 if unknown

struct XboxState {
    float lx = 0, ly = 0, rx = 0, ry = 0;  // sticks, -1..+1, +Y is up (XInput convention)
    float lt = 0, rt = 0;                  // triggers, 0..1
    std::uint16_t buttons = 0;             // XboxButton bitmask (dpad included)
};

// PlayStation state -> Xbox state. deadzone is a fraction of full deflection.
// gyro_gain > 0 folds the gyro into the right stick (gz = yaw -> X, gy = pitch -> Y),
// like a mouse ; 0 disables it.
// anti_deadzone and max_zone come from the DS4Windows-derived shaping
// (see ds4w_shaping.hpp) ; defaults keep the plain deadzone behavior
XboxState map_state(const PadState& pad, float deadzone = 0.08f, float gyro_gain = 0.0f,
                    const Remap& remap = default_remap(), float anti_deadzone = 0.0f,
                    float max_zone = 1.0f);

} // namespace ds2x
