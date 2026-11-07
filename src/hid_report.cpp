#include "hid_report.hpp"

namespace ds2x {

Device device_from_pid(std::uint16_t pid) {
    // anything we don't recognize gets treated as a DualSense; if it quacks
    // like a DS4 report-wise the parser just returns nullopt and we move on
    switch (pid) {
        case 0x05C4:  // DualShock 4 v1
        case 0x09CC:  // DualShock 4 v2
            return Device::DualShock4;
        default:
            return Device::DualSense;
    }
}

std::string_view device_name(Device d) {
    return d == Device::DualShock4 ? "DualShock 4" : "DualSense";
}

static std::int16_t read_i16(const std::uint8_t* p) {
    return static_cast<std::int16_t>(p[0] | (p[1] << 8));
}

// --- DualShock 4 -----------------------------------------------------------
// USB report 0x01:  LX LY RX RY @1..4, buttons1 @5, buttons2 @6, buttons3 @7,
//                   L2 @8, R2 @9, gyro @13..18
// BT report 0x11:   same payload but bluetooth staples 0xC0 0x00 after the
//                   report id, so everything shifts by 2.
// `base` = index of LX.
static std::optional<PadState> parse_ds4(const std::uint8_t* d, std::size_t len, std::size_t base) {
    if (len < base + 9) return std::nullopt;
    PadState s{Device::DualShock4};
    s.lx = d[base + 0]; s.ly = d[base + 1];
    s.rx = d[base + 2]; s.ry = d[base + 3];
    const std::uint8_t b1 = d[base + 4], b2 = d[base + 5], b3 = d[base + 6];
    s.l2 = d[base + 7]; s.r2 = d[base + 8];

    s.dpad = b1 & 0x0F;
    if (b1 & 0x10) s.buttons |= Square;
    if (b1 & 0x20) s.buttons |= Cross;
    if (b1 & 0x40) s.buttons |= Circle;
    if (b1 & 0x80) s.buttons |= Triangle;
    if (b2 & 0x01) s.buttons |= L1;
    if (b2 & 0x02) s.buttons |= R1;
    if (b2 & 0x04) s.buttons |= L2;
    if (b2 & 0x08) s.buttons |= R2;
    if (b2 & 0x10) s.buttons |= Create;  // this is the Share button. sony renamed it later.
    if (b2 & 0x20) s.buttons |= Options;
    if (b2 & 0x40) s.buttons |= L3;
    if (b2 & 0x80) s.buttons |= R3;
    if (b3 & 0x01) s.buttons |= PS;
    if (b3 & 0x02) s.buttons |= Touchpad;
    // gyro lives further out (USB bytes 13-18, i.e. base+12); short reports
    // just leave it at zero, not every report carries it
    if (len >= base + 18) {
        s.gx = read_i16(d + base + 12);
        s.gy = read_i16(d + base + 14);
        s.gz = read_i16(d + base + 16);
    }
    return s;
}

// --- DualSense --------------------------------------------------------------
// USB report 0x01:  LX LY RX RY @1..4, L2 @5, R2 @6, seq @7,
//                   buttons1 @8, buttons2 @9, buttons3 @10, gyro @16..21
// BT report 0x31:   same payload shifted by 1 (a lonely 0x51 after the id)
static std::optional<PadState> parse_dualsense(const std::uint8_t* d, std::size_t len, std::size_t base) {
    if (len < base + 10) return std::nullopt;
    PadState s{Device::DualSense};
    s.lx = d[base + 0]; s.ly = d[base + 1];
    s.rx = d[base + 2]; s.ry = d[base + 3];
    s.l2 = d[base + 4]; s.r2 = d[base + 5];
    const std::uint8_t b1 = d[base + 7], b2 = d[base + 8], b3 = d[base + 9];

    s.dpad = b1 & 0x0F;
    if (b1 & 0x10) s.buttons |= Square;
    if (b1 & 0x20) s.buttons |= Cross;
    if (b1 & 0x40) s.buttons |= Circle;
    if (b1 & 0x80) s.buttons |= Triangle;
    if (b2 & 0x01) s.buttons |= L1;
    if (b2 & 0x02) s.buttons |= R1;
    if (b2 & 0x04) s.buttons |= L2;
    if (b2 & 0x08) s.buttons |= R2;
    if (b2 & 0x10) s.buttons |= Create;  // the Create button. it was Share once.
    if (b2 & 0x20) s.buttons |= Options;
    if (b2 & 0x40) s.buttons |= L3;
    if (b2 & 0x80) s.buttons |= R3;
    if (b3 & 0x01) s.buttons |= PS;
    if (b3 & 0x02) s.buttons |= Touchpad;
    // gyro at USB bytes 16-21 (base+15)
    if (len >= base + 21) {
        s.gx = read_i16(d + base + 15);
        s.gy = read_i16(d + base + 17);
        s.gz = read_i16(d + base + 19);
    }
    return s;
}

Transport transport_from_path(const char* path) {
    if (!path) return Transport::Usb;
    for (const char* p = path; *p; ++p) {
        // case-insensitive "bth" anywhere in the path means bluetooth
        if ((*p == 'b' || *p == 'B') && (p[1] == 't' || p[1] == 'T') &&
            (p[2] == 'h' || p[2] == 'H'))
            return Transport::Bluetooth;
    }
    return Transport::Usb;
}

std::optional<PadState> parse_report(Device dev, Transport transport,
                                     const std::uint8_t* data, std::size_t len) {
    if (len == 0) return std::nullopt;
    const std::uint8_t rid = data[0];

    if (dev == Device::DualShock4) {
        if (rid == 0x01) return parse_ds4(data, len, 1);
        if (rid == 0x11) return parse_ds4(data, len, 3);  // BT full report, +2 shift
        return std::nullopt;
    }

    // DualSense
    if (rid == 0x31 && transport == Transport::Bluetooth)
        return parse_dualsense(data, len, 2);             // BT full report, +1 shift
    if (rid == 0x01) {
        if (transport == Transport::Bluetooth)
            // over BT the DualSense starts in "simple" mode, which is literally
            // the old DualShock 4 report. sony reused the paperwork.
            return parse_ds4(data, len, 1);
        return parse_dualsense(data, len, 1);
    }
    return std::nullopt;
}

} // namespace ds2x
