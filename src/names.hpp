#pragma once

#include <cstdint>
#include <string>

namespace ds2x {

// button name tables shared by the remap parser and anything else that
// needs to talk about buttons in plain words

int ps_button_index(const std::string& name);   // square/cross/.../touchpad ; -1 if unknown
int xbox_button_bit(const std::string& name);   // a/b/.../dpad_up ; 0 if unknown
const char* ps_button_name(int index);          // inverse of ps_button_index
std::uint16_t ps_button_bit(int index);         // 1 << index
const char* xbox_button_name(std::uint16_t bit); // inverse of xbox_button_bit ; "" if not exactly one known bit

} // namespace ds2x
