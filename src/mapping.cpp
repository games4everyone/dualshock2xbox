#include "mapping.hpp"

#include "names.hpp"

#include "ds4w_shaping.hpp"

#include <cmath>

namespace ds2x {

// HID axis byte -> -1..+1. HID sticks report 0 = up, XInput wants +1 = up,
 // so Y gets negated ; nobody downstream should ever have to think about it.
static float axis(std::uint8_t v) {
    return (static_cast<float>(v) - 127.5f) / 127.5f;
}

static float clamp1(float v) {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

Remap default_remap() {
    Remap r{};
    r[ps_button_index("cross")] = XA;
    r[ps_button_index("circle")] = XB;
    r[ps_button_index("square")] = XX;
    r[ps_button_index("triangle")] = XY;
    r[ps_button_index("l1")] = XLB;
    r[ps_button_index("r1")] = XRB;
    r[ps_button_index("l3")] = XLS;
    r[ps_button_index("r3")] = XRS;
    r[ps_button_index("create")] = XBack;
    r[ps_button_index("options")] = XStart;
    r[ps_button_index("ps")] = XGuide;
    // l2/r2 are analog triggers, not remappable buttons ; touchpad click defaults
    // to dropped, but remap_touchpad=rs (or anything) brings it back
    return r;
}

bool parse_remap(const std::string& ps_name, const std::string& xbox_name, Remap& out) {
    int i = ps_button_index(ps_name);
    int bit = xbox_button_bit(xbox_name);
    // note: names.cpp owns the tables now
    if (i < 0 || bit == 0) return false;
    if (ps_name == "l2" || ps_name == "r2") return false;  // axes, not buttons
    out[i] = static_cast<std::uint16_t>(bit);
    return true;
}

XboxState map_state(const PadState& pad, float deadzone, float gyro_gain, const Remap& remap,
                    float anti_deadzone, float max_zone) {
    XboxState out;

    out.lx = axis(pad.lx);
    out.ly = -axis(pad.ly);  // flip: HID up=0, XInput up=+1
    out.rx = axis(pad.rx);
    out.ry = -axis(pad.ry);
    // DS4Windows-derived stick shaping (deadzone + anti-deadzone + max zone)
    StickShaping shaping{deadzone, anti_deadzone, max_zone};
    shape_stick(out.lx, out.ly, shaping);
    shape_stick(out.rx, out.ry, shaping);

    // gyro on top of the right stick, clamped ; yaw turns you, pitch looks up/down
    if (gyro_gain > 0.0f) {
        out.rx = clamp1(out.rx + (pad.gz / 32767.0f) * gyro_gain);
        out.ry = clamp1(out.ry - (pad.gy / 32767.0f) * gyro_gain);
    }

    out.lt = pad.l2 / 255.0f;
    out.rt = pad.r2 / 255.0f;

    // table-driven remap ; anything mapping to zero is dropped on the floor
    for (int i = 0; i < 14; ++i)
        if ((pad.buttons & (1u << i)) && remap[i])
            out.buttons |= remap[i];

    switch (pad.dpad) {
        case 0: out.buttons |= XDpadUp; break;
        case 1: out.buttons |= XDpadUp | XDpadRight; break;
        case 2: out.buttons |= XDpadRight; break;
        case 3: out.buttons |= XDpadDown | XDpadRight; break;
        case 4: out.buttons |= XDpadDown; break;
        case 5: out.buttons |= XDpadDown | XDpadLeft; break;
        case 6: out.buttons |= XDpadLeft; break;
        case 7: out.buttons |= XDpadUp | XDpadLeft; break;
        default: break;  // 8 = released
    }
    return out;
}

} // namespace ds2x
