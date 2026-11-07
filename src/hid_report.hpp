#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ds2x {

enum class Device { DualShock4, DualSense };
enum class Transport { Usb, Bluetooth };

// button bits in PadState::buttons
enum Button : std::uint16_t {
    Square   = 1u << 0,
    Cross    = 1u << 1,
    Circle   = 1u << 2,
    Triangle = 1u << 3,
    L1       = 1u << 4,
    R1       = 1u << 5,
    L2       = 1u << 6,  // digital bit; analog value is in l2
    R2       = 1u << 7,
    Create   = 1u << 8,  // "Share" on DS4, renamed "Create" on DualSense. same button.
    Options  = 1u << 9,
    L3       = 1u << 10,
    R3       = 1u << 11,
    PS       = 1u << 12,
    Touchpad = 1u << 13,
};

// one decoded controller sample
struct PadState {
    Device device;
    std::uint8_t lx = 128, ly = 128, rx = 128, ry = 128;  // sticks, 0..255, 128 = center, Y: 0 = up
    std::uint8_t l2 = 0, r2 = 0;                          // analog triggers, 0..255
    std::uint8_t dpad = 8;                                // hat: 0=N 1=NE ... 7=NW, 8 = released
    std::uint16_t buttons = 0;
    std::int16_t gx = 0, gy = 0, gz = 0;                  // gyro, signed; gz is yaw (turning left/right)
};

constexpr std::uint16_t SONY_VID = 0x054C;

// DualShock 4 (05C4, 09CC), DualSense (0CE6), DualSense Edge (0DF2, same layout)
Device device_from_pid(std::uint16_t pid);
std::string_view device_name(Device d);

// Decode one raw HID input report. Returns nullopt for unknown/garbage reports.
// `data[0]` is the report id (hidapi keeps it in the buffer).
std::optional<PadState> parse_report(Device dev, Transport transport,
                                     const std::uint8_t* data, std::size_t len);

// guess the transport from a HID device path ; on Windows the path contains
// "BTH" for bluetooth devices. anything else is treated as cable.
Transport transport_from_path(const char* path);

} // namespace ds2x
