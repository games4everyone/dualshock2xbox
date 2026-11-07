#include "output_report.hpp"

#include <cstring>

namespace ds2x {

std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

// append the sony BT trailer: CRC-32 over 0xA2 + report[0..len-1], little-endian
static void append_bt_crc(std::vector<std::uint8_t>& r) {
    const std::uint8_t seed = 0xA2;
    std::uint32_t crc = crc32(&seed, 1);
    // crc32() is one-shot, so fold the report in manually the same way
    crc = 0xFFFFFFFFu;
    crc ^= seed;
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    for (std::uint8_t byte : r) {
        crc ^= byte;
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    crc = ~crc;
    for (int i = 0; i < 4; ++i) r.push_back(static_cast<std::uint8_t>(crc >> (8 * i)));
}

// --- DualShock 4 -------------------------------------------------------------
// USB output 0x05 (32 bytes): motors at [4] weak / [5] strong.
// BT output 0x11 (78 bytes): [1]=0xC0 (output + CRC present), [3]=0x03 effect
// mask (rumble only, light bar left alone), [6]=weak, [7]=strong, CRC at [74..77].
std::vector<std::uint8_t> ds4_rumble_report(Transport t, std::uint8_t weak,
                                            std::uint8_t strong, std::uint8_t& seq) {
    (void)seq;  // ds4 bt has no sequence nibble
    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> r(74, 0);
        r[0] = 0x11; r[1] = 0xC0;
        r[3] = 0x03;  // rumble only. we don't touch the light bar.
        r[6] = weak; r[7] = strong;
        append_bt_crc(r);
        return r;
    }
    std::vector<std::uint8_t> r(32, 0);
    r[0] = 0x05;
    r[4] = weak; r[5] = strong;
    return r;
}

// --- DualSense ---------------------------------------------------------------
// USB output 0x02 (48 bytes): [1]=valid flags (0x03 = both rumble motors),
// [3]=weak/right motor, [4]=strong/left motor.
// BT output 0x31 (78 bytes): [1] = (seq << 4) | 0x10 tag, then the 47-byte USB
// payload, padded to 74, CRC at [74..77].
std::vector<std::uint8_t> dualsense_rumble_report(Transport t, std::uint8_t weak,
                                                  std::uint8_t strong, std::uint8_t& seq) {
    std::vector<std::uint8_t> payload(47, 0);
    payload[0] = 0x03;  // valid flags 0: both rumble motors
    payload[2] = weak;   // byte 3 of the full report
    payload[3] = strong; // byte 4 of the full report

    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> r(74, 0);
        r[0] = 0x31;
        r[1] = static_cast<std::uint8_t>(((seq++ & 0x0F) << 4) | 0x10);
        std::memcpy(r.data() + 2, payload.data(), payload.size());
        append_bt_crc(r);
        return r;
    }
    std::vector<std::uint8_t> r(48, 0);
    r[0] = 0x02;
    std::memcpy(r.data() + 1, payload.data(), payload.size());
    return r;
}

std::vector<std::uint8_t> rumble_report(Device dev, Transport t, std::uint8_t weak,
                                        std::uint8_t strong, std::uint8_t& seq) {
    return dev == Device::DualShock4 ? ds4_rumble_report(t, weak, strong, seq)
                                     : dualsense_rumble_report(t, weak, strong, seq);
}

// --- light bar ---------------------------------------------------------------
// DS4 USB 0x05: flags byte [1] = 0x02 (led only), RGB at [6..8].
// DS4 BT 0x11: mask 0x02 at [3], RGB at [8..10], CRC trailer.
// DualSense: 47-byte payload, flags2 0x02 at payload[38], RGB at payload[43..45];
// wrapped as USB 0x02 or BT 0x31 like the rumble report.
static std::vector<std::uint8_t> ds4_lightbar_report(Transport t, std::uint8_t r,
                                                     std::uint8_t g, std::uint8_t b) {
    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> v(74, 0);
        v[0] = 0x11; v[1] = 0xC0;
        v[3] = 0x02;  // led only ; motors left alone
        v[8] = r; v[9] = g; v[10] = b;
        append_bt_crc(v);
        return v;
    }
    std::vector<std::uint8_t> v(32, 0);
    v[0] = 0x05;
    v[1] = 0x02;
    v[6] = r; v[7] = g; v[8] = b;
    return v;
}

static std::vector<std::uint8_t> dualsense_lightbar_report(Transport t, std::uint8_t r,
                                                           std::uint8_t g, std::uint8_t b,
                                                           std::uint8_t& seq) {
    std::vector<std::uint8_t> payload(47, 0);
    payload[38] = 0x02;  // valid flags 2: lightbar setup
    payload[43] = r; payload[44] = g; payload[45] = b;

    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> v(74, 0);
        v[0] = 0x31;
        v[1] = static_cast<std::uint8_t>(((seq++ & 0x0F) << 4) | 0x10);
        std::memcpy(v.data() + 2, payload.data(), payload.size());
        append_bt_crc(v);
        return v;
    }
    std::vector<std::uint8_t> v(48, 0);
    v[0] = 0x02;
    std::memcpy(v.data() + 1, payload.data(), payload.size());
    return v;
}

std::vector<std::uint8_t> lightbar_report(Device dev, Transport t, std::uint8_t r,
                                          std::uint8_t g, std::uint8_t b,
                                          std::uint8_t& seq) {
    return dev == Device::DualShock4 ? ds4_lightbar_report(t, r, g, b)
                                     : dualsense_lightbar_report(t, r, g, b, seq);
}

// DualSense player LED bitmask for players 1..5 (centre LED is bit 2).
// the DS4 has no player lights, so there is nothing to send.
std::vector<std::uint8_t> player_led_report(Transport t, int player, std::uint8_t& seq) {
    static const std::uint8_t patterns[5] = {0x04, 0x0A, 0x15, 0x1B, 0x1F};
    if (player < 1 || player > 5) return {};
    const std::uint8_t pattern = patterns[player - 1] | 0x20;  // 0x20 = change now, no fade

    std::vector<std::uint8_t> payload(47, 0);
    payload[0] = 0x10;   // valid flag 1, bit 4: player LEDs
    payload[42] = pattern;

    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> v(74, 0);
        v[0] = 0x31;
        v[1] = static_cast<std::uint8_t>(((seq++ & 0x0F) << 4) | 0x10);
        std::memcpy(v.data() + 2, payload.data(), payload.size());
        append_bt_crc(v);
        return v;
    }
    std::vector<std::uint8_t> v(48, 0);
    v[0] = 0x02;
    std::memcpy(v.data() + 1, payload.data(), payload.size());
    return v;
}

// --- adaptive triggers -------------------------------------------------------
static void put_u16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
static void put_u32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

// pack a uniform strength for zones [start..9] into the 3-bits-per-zone u32
static std::uint32_t pack_strengths(int start, int strength) {
    std::uint32_t v = 0;
    const std::uint32_t field = static_cast<std::uint32_t>((strength - 1) & 7);
    for (int z = start; z < 10; ++z) v |= field << (3 * z);
    return v;
}

TriggerEffect trigger_off() { return {}; }

TriggerEffect trigger_feedback(int start_zone, int strength) {
    TriggerEffect e;
    if (strength <= 0) return e;  // strength 0 means off
    start_zone = start_zone < 0 ? 0 : (start_zone > 9 ? 9 : start_zone);
    strength = strength < 1 ? 1 : (strength > 8 ? 8 : strength);
    e.mode = 0x21;
    e.zones = static_cast<std::uint16_t>(0x3FFu << start_zone);  // zones start..9
    e.strengths = pack_strengths(start_zone, strength);
    return e;
}

TriggerEffect trigger_weapon(int start_zone, int end_zone, int strength) {
    TriggerEffect e;
    if (strength <= 0) return e;
    start_zone = start_zone < 2 ? 2 : (start_zone > 7 ? 7 : start_zone);
    end_zone = end_zone < start_zone + 1 ? start_zone + 1 : (end_zone > 8 ? 8 : end_zone);
    strength = strength < 1 ? 1 : (strength > 8 ? 8 : strength);
    e.mode = 0x25;
    e.zones = static_cast<std::uint16_t>((1u << start_zone) | (1u << end_zone));
    e.strengths = pack_strengths(start_zone, strength) & pack_strengths(end_zone, strength);
    return e;
}

TriggerEffect trigger_vibration(int start_zone, int amplitude, int freq_hz) {
    TriggerEffect e;
    if (amplitude <= 0) return e;
    start_zone = start_zone < 0 ? 0 : (start_zone > 9 ? 9 : start_zone);
    amplitude = amplitude < 1 ? 1 : (amplitude > 8 ? 8 : amplitude);
    e.mode = 0x26;
    e.zones = static_cast<std::uint16_t>(0x3FFu << start_zone);
    e.strengths = pack_strengths(start_zone, amplitude);
    e.frequency = static_cast<std::uint8_t>(freq_hz > 255 ? 255 : freq_hz);
    return e;
}

static void put_effect(std::uint8_t* block, const TriggerEffect& e) {
    block[0] = e.mode;
    put_u16(block + 1, e.zones);
    put_u32(block + 3, e.strengths);
    block[7] = e.frequency;
    // bytes 8-10 stay zero
}

std::vector<std::uint8_t> dualsense_trigger_report(Transport t, const TriggerEffect& left,
                                                   const TriggerEffect& right,
                                                   std::uint8_t& seq) {
    std::vector<std::uint8_t> payload(47, 0);
    payload[0] = 0x0C;  // valid flags 0: both trigger effects (right 0x04, left 0x08)
    put_effect(payload.data() + 10, right);  // right trigger block, full report bytes 11-21
    put_effect(payload.data() + 21, left);   // left trigger block, full report bytes 22-32

    if (t == Transport::Bluetooth) {
        std::vector<std::uint8_t> v(74, 0);
        v[0] = 0x31;
        v[1] = static_cast<std::uint8_t>(((seq++ & 0x0F) << 4) | 0x10);
        std::memcpy(v.data() + 2, payload.data(), payload.size());
        append_bt_crc(v);
        return v;
    }
    std::vector<std::uint8_t> v(48, 0);
    v[0] = 0x02;
    std::memcpy(v.data() + 1, payload.data(), payload.size());
    return v;
}

} // namespace ds2x
