// mapping tests: buttons, axes, deadzone, triggers, dpad, gyro, remap

#include "test_util.hpp"

#include "ds4w_shaping.hpp"
#include "mapping.hpp"

#include <cmath>

using namespace ds2x;

void test_mapping_buttons() {
    PadState p{Device::DualSense};
    p.buttons = Cross | Circle | Square | Triangle | L1 | R1 | L3 | R3 | Create | Options | PS;
    auto x = map_state(p);
    CHECK(x.buttons & XA && x.buttons & XB && x.buttons & XX && x.buttons & XY);
    CHECK(x.buttons & XLB && x.buttons & XRB && x.buttons & XLS && x.buttons & XRS);
    CHECK(x.buttons & XBack && x.buttons & XStart && x.buttons & XGuide);
}

void test_mapping_touchpad_ignored_by_default() {
    PadState p{Device::DualShock4};
    p.buttons = Touchpad;
    auto x = map_state(p);
    CHECK(x.buttons == 0);
}

void test_mapping_axes() {
    PadState p{Device::DualShock4};
    p.lx = 255; p.ly = 0; p.rx = 0; p.ry = 128;
    auto x = map_state(p, 0.0f);
    CHECK(std::fabs(x.lx - 1.0f) < 1e-4);
    CHECK(std::fabs(x.ly - 1.0f) < 1e-4);   // HID up=0 becomes XInput up=+1
    CHECK(std::fabs(x.rx + 1.0f) < 1e-4);
    CHECK(std::fabs(x.ry) < 0.01f);

    p.lx = 128; p.ly = 128;
    x = map_state(p, 0.0f);
    CHECK(std::fabs(x.lx) < 0.01f && std::fabs(x.ly) < 0.01f);
}

void test_mapping_deadzone() {
    PadState p{Device::DualShock4};
    p.lx = 128 + 5;
    auto x = map_state(p, 0.08f);
    CHECK(x.lx == 0.0f);
    x = map_state(p, 0.0f);
    CHECK(x.lx > 0.0f);
}

void test_mapping_triggers_and_dpad() {
    PadState p{Device::DualSense};
    p.l2 = 255; p.r2 = 128; p.dpad = 1;
    auto x = map_state(p);
    CHECK(std::fabs(x.lt - 1.0f) < 1e-4);
    CHECK(std::fabs(x.rt - 128 / 255.0f) < 1e-4);
    CHECK((x.buttons & XDpadUp) && (x.buttons & XDpadRight));
    CHECK(!(x.buttons & XDpadDown) && !(x.buttons & XDpadLeft));
}

void test_gyro_mapping() {
    // sony's order is pitch, yaw, roll: gx / gy / gz. yaw turns the camera (rx),
    // pitch looks up and down (ry), roll does nothing useful for aiming
    PadState p{Device::DualSense};
    p.gy = 16384;  // yaw
    auto off = map_state(p, 0.08f, 0.0f);
    CHECK(off.rx == 0.0f);
    auto on = map_state(p, 0.08f, 1.0f);
    CHECK(on.rx > 0.4f && on.rx < 0.6f);
    p.gy = 32767;
    on = map_state(p, 0.08f, 5.0f);
    CHECK(on.rx == 1.0f);

    PadState pitch{Device::DualSense};
    pitch.gx = 16384;
    auto pm = map_state(pitch, 0.08f, 1.0f);
    CHECK(pm.rx == 0.0f && pm.ry < -0.4f && pm.ry > -0.6f);  // pitch only moves Y

    PadState roll{Device::DualSense};
    roll.gz = 32767;
    auto rm = map_state(roll, 0.08f, 5.0f);
    CHECK(rm.rx == 0.0f && rm.ry == 0.0f);  // rolling the pad like a steering wheel steers nothing

    // invert flags flip direction without touching anything else
    PadState yaw{Device::DualSense};
    yaw.gy = 16384;
    auto inv = map_state(yaw, 0.08f, 1.0f, default_remap(), 0.0f, 1.0f, true, false);
    CHECK(inv.rx < -0.4f && inv.rx > -0.6f);
    PadState both{Device::DualSense};
    both.gx = 16384;
    auto inv2 = map_state(both, 0.08f, 1.0f, default_remap(), 0.0f, 1.0f, false, true);
    CHECK(inv2.ry > 0.4f && inv2.ry < 0.6f);
}

void test_ds4w_shaping() {
    PadState p{Device::DualShock4};
    p.lx = 128 + 20;  // ~0.157 deflection
    // plain deadzone 0.08: passes, rescaled
    auto x = map_state(p, 0.08f);
    CHECK(x.lx > 0.0f && x.lx < 0.3f);
    // anti-deadzone 0.25: smallest movement past the zone starts at 0.25
    x = map_state(p, 0.08f, 0.0f, default_remap(), 0.25f, 1.0f);
    CHECK(x.lx >= 0.25f);
    // max zone 0.5: half deflection already means full output
    p.lx = 200;  // ~0.57 deflection: past the 0.5 max zone after deadzone rescale
    x = map_state(p, 0.08f, 0.0f, default_remap(), 0.0f, 0.5f);
    CHECK(x.lx == 1.0f);
}

void test_remap() {
    PadState p{Device::DualSense};
    p.buttons = Cross | Touchpad;
    auto x = map_state(p);
    CHECK(x.buttons == XA);  // touchpad dropped by default

    Remap r = default_remap();
    CHECK(parse_remap("cross", "b", r));
    CHECK(parse_remap("touchpad", "rs", r));
    CHECK(!parse_remap("cross", "notabutton", r));
    CHECK(!parse_remap("l2", "a", r));
    x = map_state(p, 0.08f, 0.0f, r);
    CHECK((x.buttons & XB) && (x.buttons & XRS) && !(x.buttons & XA));

    Remap r2 = default_remap();
    CHECK(parse_remap("options", "dpad_up", r2));
    p.buttons = Options;
    x = map_state(p, 0.08f, 0.0f, r2);
    CHECK((x.buttons & XDpadUp) && !(x.buttons & XStart));
}

// the old shaping zeroed each axis separately, so a nearly-straight push lost its
// minor axis entirely (x=0.05, y=0.9 became pure vertical). direction must survive.
void test_stick_direction_preserved() {
    StickShaping sh;
    sh.deadzone = 0.08f;
    float x = 0.05f, y = 0.9f;
    shape_stick(x, y, sh);
    CHECK(x > 0.0f);                       // minor axis NOT snapped to zero
    CHECK(std::fabs(x / y - 0.05f / 0.9f) < 1e-4);  // same angle in, same angle out

    // inside the deadzone: dead on both axes
    x = 0.04f; y = 0.04f;
    shape_stick(x, y, sh);
    CHECK(x == 0.0f && y == 0.0f);

    // the 45-degree push keeps its 45 degrees
    x = 0.4f; y = 0.4f;
    shape_stick(x, y, sh);
    CHECK(std::fabs(x - y) < 1e-6 && x > 0.0f);

    // output starts at zero right at the deadzone edge (no jump without anti-deadzone)
    x = 0.0801f; y = 0.0f;
    shape_stick(x, y, sh);
    CHECK(x >= 0.0f && x < 0.01f);

    // nonsense settings must not divide by zero or produce NaN
    StickShaping bad;
    bad.deadzone = 1.0f; bad.max_zone = 0.0f; bad.anti_deadzone = 5.0f;
    x = 0.99f; y = 0.2f;
    shape_stick(x, y, bad);
    CHECK(x == x && y == y && std::fabs(x) <= 1.0f && std::fabs(y) <= 1.0f);

    // corners stay reachable (square-gated input shouldn't be squeezed to 0.707)
    x = 1.0f; y = 1.0f;
    shape_stick(x, y, StickShaping{});
    CHECK(x == 1.0f && y == 1.0f);
}

void test_xusb_conversion() {
    // full deflection must be full: truncating 0.99999994f lost a count
    CHECK(to_xusb_trigger(1.0f) == 255 && to_xusb_trigger(0.0f) == 0);
    CHECK(to_xusb_trigger(128 / 255.0f) == 128);   // round trip of a pad value
    for (int v = 0; v < 256; ++v) CHECK(to_xusb_trigger(v / 255.0f) == v);
    CHECK(to_xusb_trigger(-1.0f) == 0 && to_xusb_trigger(7.0f) == 255);
    CHECK(to_xusb_axis(1.0f) == 32767 && to_xusb_axis(-1.0f) == -32767 && to_xusb_axis(0.0f) == 0);
    CHECK(to_xusb_axis(0.5f) == 16384);  // rounds, not truncates (16383.5)
    CHECK(to_xusb_axis(5.0f) == 32767 && to_xusb_axis(-5.0f) == -32767);
    CHECK(to_xusb_axis(std::nanf("")) == 0 && to_xusb_trigger(std::nanf("")) == 0);
}
