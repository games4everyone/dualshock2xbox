// output report tests: crc32, rumble, light bar, player LEDs, adaptive triggers

#include "test_util.hpp"

#include "output_report.hpp"

#include <vector>

using namespace ds2x;

static std::uint32_t stored_crc(const std::vector<std::uint8_t>& r) {
    std::vector<std::uint8_t> seeded = {0xA2};
    seeded.insert(seeded.end(), r.begin(), r.begin() + 74);
    std::uint32_t crc = crc32(seeded.data(), seeded.size());
    std::uint32_t stored = r[74] | (r[75] << 8) | (r[76] << 16) |
                           (static_cast<std::uint32_t>(r[77]) << 24);
    CHECK(crc == stored);
    return crc;
}

void test_crc32() {
    const char* s = "123456789";
    CHECK(crc32(reinterpret_cast<const std::uint8_t*>(s), 9) == 0xCBF43926u);

    // incremental form must agree with the one-shot form however the data is split
    const auto* d = reinterpret_cast<const std::uint8_t*>(s);
    for (std::size_t cut = 0; cut <= 9; ++cut) {
        std::uint32_t st = 0xFFFFFFFFu;
        st = crc32_update(st, d, cut);
        st = crc32_update(st, d + cut, 9 - cut);
        CHECK(~st == 0xCBF43926u);
    }
}

void test_rumble_reports() {
    std::uint8_t seq = 0;
    auto ds4usb = ds4_rumble_report(Transport::Usb, 10, 200, seq);
    CHECK(ds4usb.size() == 32 && ds4usb[0] == 0x05 && ds4usb[4] == 10 && ds4usb[5] == 200);
    CHECK(ds4usb[1] == 0x01);  // valid flag "motors": without it the pad ignores the whole report

    auto ds4bt = ds4_rumble_report(Transport::Bluetooth, 10, 200, seq);
    CHECK(ds4bt.size() == 78 && ds4bt[0] == 0x11 && ds4bt[1] == 0xC0 &&
          ds4bt[3] == 0x01 && ds4bt[6] == 10 && ds4bt[7] == 200);
    CHECK(ds4bt[3] == 0x01);  // motors only ; 0x03 would also apply colour 0,0,0 = light bar off
    stored_crc(ds4bt);

    auto dsusb = dualsense_rumble_report(Transport::Usb, 10, 200, seq);
    CHECK(dsusb.size() == 48 && dsusb[0] == 0x02 && dsusb[1] == 0x03 &&
          dsusb[3] == 10 && dsusb[4] == 200);

    auto dsbt = dualsense_rumble_report(Transport::Bluetooth, 10, 200, seq);
    CHECK(dsbt.size() == 78 && dsbt[0] == 0x31 && dsbt[1] == 0x10);
    CHECK(dsbt[4] == 10 && dsbt[5] == 200);
    CHECK(seq == 1);
    stored_crc(dsbt);
}

void test_lightbar_reports() {
    std::uint8_t seq = 0;
    auto a = lightbar_report(Device::DualShock4, Transport::Usb, 10, 20, 30, seq);
    CHECK(a.size() == 32 && a[0] == 0x05 && a[1] == 0x02 && a[6] == 10 && a[7] == 20 && a[8] == 30);

    auto b = lightbar_report(Device::DualShock4, Transport::Bluetooth, 10, 20, 30, seq);
    CHECK(b.size() == 78 && b[0] == 0x11 && b[3] == 0x02 && b[8] == 10 && b[9] == 20 && b[10] == 30);
    stored_crc(b);

    auto c = lightbar_report(Device::DualSense, Transport::Usb, 10, 20, 30, seq);
    // payload[1] = valid_flag1 (0x04 = apply light bar), payload[44..46] = R,G,B
    // (payload index + 1 on USB because of the report id)
    CHECK(c.size() == 48 && c[0] == 0x02 && c[2] == 0x04 && c[45] == 10 && c[46] == 20 && c[47] == 30);

    auto d = lightbar_report(Device::DualSense, Transport::Bluetooth, 10, 20, 30, seq);
    CHECK(d.size() == 78 && d[0] == 0x31 && d[3] == 0x04 && d[46] == 10 && d[47] == 20 && d[48] == 30);
    stored_crc(d);
}

void test_player_led_reports() {
    std::uint8_t seq = 0;
    auto p1 = player_led_report(Transport::Usb, 1, seq);
    // valid_flag1 (payload[1]) bit 4 = player LEDs ; pattern at payload[43]
    CHECK(p1.size() == 48 && p1[0] == 0x02 && p1[1] == 0x00 && p1[2] == 0x10 && p1[44] == (0x04 | 0x20));
    auto p5 = player_led_report(Transport::Usb, 5, seq);
    CHECK(p5[44] == (0x1F | 0x20));
    CHECK(player_led_report(Transport::Usb, 0, seq).empty());
    CHECK(player_led_report(Transport::Usb, 6, seq).empty());
    auto bt = player_led_report(Transport::Bluetooth, 3, seq);
    CHECK(bt.size() == 78 && bt[0] == 0x31 && bt[3] == 0x10 && bt[45] == (0x15 | 0x20));
    stored_crc(bt);
}

void test_trigger_reports() {
    auto fx = trigger_feedback(2, 6);
    CHECK(fx.mode == 0x21 && fx.zones == 0x3FC);  // zones 2..9 ; the trigger has no zone 10 or 11
    std::uint32_t expect = 0;
    for (int z = 2; z < 10; ++z) expect |= 5u << (3 * z);
    CHECK(fx.strengths == expect);

    auto off = trigger_off();
    CHECK(off.mode == 0x05 && off.zones == 0 && off.strengths == 0);
    CHECK(trigger_feedback(0, 0).mode == 0x05);

    auto w = trigger_weapon(4, 6, 8);
    CHECK(w.mode == 0x25 && w.zones == ((1u << 4) | (1u << 6)));
    CHECK(w.strengths == 7);                      // ONE value, strength-1, not per-zone packing
    CHECK(trigger_weapon(4, 6, 1).strengths == 0);
    for (int start = 0; start < 10; ++start) {    // masks never leak past the 10 real zones
        CHECK((trigger_feedback(start, 4).zones & ~0x3FFu) == 0);
        CHECK((trigger_vibration(start, 4, 30).zones & ~0x3FFu) == 0);
    }

    std::uint8_t seq = 0;
    auto r = dualsense_trigger_report(Transport::Usb, fx, off, seq);
    CHECK(r.size() == 48 && r[0] == 0x02 && r[1] == 0x0C);
    CHECK(r[11] == 0x05);  // right trigger block at byte 11
    CHECK(r[22] == 0x21);  // left trigger block at byte 22
    CHECK(r[23] == static_cast<std::uint8_t>(fx.zones) && r[24] == static_cast<std::uint8_t>(fx.zones >> 8));

    // the weapon strength lands in the byte right after the zone mask
    auto rw = dualsense_trigger_report(Transport::Usb, w, off, seq);  // (left, right)
    CHECK(rw[22] == 0x25 && rw[25] == 7);

    auto bt = dualsense_trigger_report(Transport::Bluetooth, fx, off, seq);
    CHECK(bt.size() == 78 && bt[0] == 0x31 && bt[12] == 0x05 && bt[23] == 0x21);
    stored_crc(bt);
}
