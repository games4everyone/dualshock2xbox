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
    PadState p{Device::DualSense};
    p.gz = 16384;
    auto off = map_state(p, 0.08f, 0.0f);
    CHECK(off.rx == 0.0f);
    auto on = map_state(p, 0.08f, 1.0f);
    CHECK(on.rx > 0.4f && on.rx < 0.6f);
    p.gz = 32767;
    on = map_state(p, 0.08f, 5.0f);
    CHECK(on.rx == 1.0f);
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
