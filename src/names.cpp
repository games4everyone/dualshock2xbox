#include "names.hpp"

#include "mapping.hpp"

namespace ds2x {

// order must match the Button bit order in hid_report.hpp
static const char* kPsNames[14] = {
    "square", "cross", "circle", "triangle", "l1", "r1", "l2", "r2",
    "create", "options", "l3", "r3", "ps", "touchpad",
};
static const char* kXboxNames[] = {
    "dpad_up", "dpad_down", "dpad_left", "dpad_right", "start", "back",
    "ls", "rs", "lb", "rb", "guide", "a", "b", "x", "y",
};
static const std::uint16_t kXboxBits[] = {
    XDpadUp, XDpadDown, XDpadLeft, XDpadRight, XStart, XBack,
    XLS, XRS, XLB, XRB, XGuide, XA, XB, XX, XY,
};

int ps_button_index(const std::string& name) {
    for (int i = 0; i < 14; ++i) if (name == kPsNames[i]) return i;
    return -1;
}
int xbox_button_bit(const std::string& name) {
    for (int i = 0; i < 15; ++i) if (name == kXboxNames[i]) return kXboxBits[i];
    return 0;
}
const char* xbox_button_name(std::uint16_t bit) {
    for (int i = 0; i < 15; ++i) if (kXboxBits[i] == bit) return kXboxNames[i];
    return "";
}
const char* ps_button_name(int index) {
    return (index >= 0 && index < 14) ? kPsNames[index] : "";
}
std::uint16_t ps_button_bit(int index) {
    return (index >= 0 && index < 14) ? static_cast<std::uint16_t>(1u << index) : 0;
}

} // namespace ds2x
