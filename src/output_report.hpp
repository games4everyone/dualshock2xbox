#pragma once

#include "hid_report.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ds2x {

// standard CRC-32 (zlib flavour: poly 0xEDB88320, init/xorout 0xFFFFFFFF).
// sony's bluetooth frames want this over 0xA2 + report bytes.
std::uint32_t crc32(const std::uint8_t* data, std::size_t len);
// incremental form: start from 0xFFFFFFFF, feed chunks, finish with ~state
std::uint32_t crc32_update(std::uint32_t state, const std::uint8_t* data, std::size_t len);

// rumble output reports. weak = small/high-frequency motor, strong = large one.
// BT variants append the CRC trailer; `seq` is the BT sequence nibble and is
// incremented per call (pass a persistent counter).
std::vector<std::uint8_t> ds4_rumble_report(Transport t, std::uint8_t weak,
                                            std::uint8_t strong, std::uint8_t& seq);
std::vector<std::uint8_t> dualsense_rumble_report(Transport t, std::uint8_t weak,
                                                  std::uint8_t strong, std::uint8_t& seq);

std::vector<std::uint8_t> rumble_report(Device dev, Transport t, std::uint8_t weak,
                                        std::uint8_t strong, std::uint8_t& seq);

// light bar / LED color output reports (no rumble, just the glow)
std::vector<std::uint8_t> lightbar_report(Device dev, Transport t, std::uint8_t r,
                                          std::uint8_t g, std::uint8_t b,
                                          std::uint8_t& seq);

// DualSense player LEDs (1..5) ; empty vector for DS4, which has no such lights
std::vector<std::uint8_t> player_led_report(Transport t, int player, std::uint8_t& seq);

// --- DualSense adaptive triggers ---------------------------------------------
// one trigger effect = mode byte + 10 parameter bytes. the well-known modes:
//   0x05 off, 0x21 feedback (steady resistance), 0x25 weapon (click at a zone),
//   0x26 vibration. parameters: u16 LE active-zone bitmask at bytes 1-2,
//   u32 LE strengths at bytes 3-6 (3 bits per zone, value = strength-1),
//   vibration frequency in Hz at byte 7.
struct TriggerEffect {
    std::uint8_t mode = 0x05;      // 0x05 = off
    std::uint16_t zones = 0;       // bitmask of the 10 zones
    std::uint32_t strengths = 0;   // 3 bits per zone
    std::uint8_t frequency = 0;    // vibration only
};

TriggerEffect trigger_off();
TriggerEffect trigger_feedback(int start_zone, int strength);   // zones start..9 resist
TriggerEffect trigger_weapon(int start_zone, int end_zone, int strength);
TriggerEffect trigger_vibration(int start_zone, int amplitude, int freq_hz);

// both triggers at once ; DS4 has no adaptive triggers, returns empty
std::vector<std::uint8_t> dualsense_trigger_report(Transport t, const TriggerEffect& left,
                                                   const TriggerEffect& right,
                                                   std::uint8_t& seq);

} // namespace ds2x
