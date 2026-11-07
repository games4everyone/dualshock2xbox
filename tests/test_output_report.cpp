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
}

void test_rumble_reports() {
    std::uint8_t seq = 0;
    auto ds4usb = ds4_rumble_report(Transport::Usb, 10, 200, seq);
    CHECK(ds4usb.size() == 32 && ds4usb[0] == 0x05 && ds4usb[4] == 10 && ds4usb[5] == 200);

    auto ds4bt = ds4_rumble_report(Transport::Bluetooth, 10, 200, seq);
    CHECK(ds4bt.size() == 78 && ds4bt[0] == 0x11 && ds4bt[1] == 0xC0 &&
          ds4bt[3] == 0x03 && ds4bt[6] == 10 && ds4bt[7] == 200);
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
    CHECK(c.size() == 48 && c[0] == 0x02 && c[39] == 0x02 && c[44] == 10 && c[45] == 20 && c[46] == 30);

    auto d = lightbar_report(Device::DualSense, Transport::Bluetooth, 10, 20, 30, seq);
    CHECK(d.size() == 78 && d[0] == 0x31 && d[45] == 10 && d[46] == 20 && d[47] == 30);
    stored_crc(d);
}

void test_player_led_reports() {
    std::uint8_t seq = 0;
    auto p1 = player_led_report(Transport::Usb, 1, seq);
    CHECK(p1.size() == 48 && p1[0] == 0x02 && p1[1] == 0x10 && p1[43] == (0x04 | 0x20));
    auto p5 = player_led_report(Transport::Usb, 5, seq);
    CHECK(p5[43] == (0x1F | 0x20));
    CHECK(player_led_report(Transport::Usb, 0, seq).empty());
    CHECK(player_led_report(Transport::Usb, 6, seq).empty());
    auto bt = player_led_report(Transport::Bluetooth, 3, seq);
    CHECK(bt.size() == 78 && bt[0] == 0x31 && bt[44] == (0x15 | 0x20));
    stored_crc(bt);
}

void test_trigger_reports() {
    auto fx = trigger_feedback(2, 6);
    CHECK(fx.mode == 0x21 && fx.zones == (0x3FF << 2));
    std::uint32_t expect = 0;
    for (int z = 2; z < 10; ++z) expect |= 5u << (3 * z);
    CHECK(fx.strengths == expect);

    auto off = trigger_off();
    CHECK(off.mode == 0x05 && off.zones == 0 && off.strengths == 0);
    CHECK(trigger_feedback(0, 0).mode == 0x05);

    auto w = trigger_weapon(4, 6, 8);
    CHECK(w.mode == 0x25 && w.zones == ((1u << 4) | (1u << 6)));

    std::uint8_t seq = 0;
    auto r = dualsense_trigger_report(Transport::Usb, fx, off, seq);
    CHECK(r.size() == 48 && r[0] == 0x02 && r[1] == 0x0C);
    CHECK(r[11] == 0x05);  // right trigger block at byte 11
    CHECK(r[22] == 0x21);  // left trigger block at byte 22
    CHECK(r[23] == static_cast<std::uint8_t>(fx.zones) && r[24] == static_cast<std::uint8_t>(fx.zones >> 8));

    auto bt = dualsense_trigger_report(Transport::Bluetooth, fx, off, seq);
    CHECK(bt.size() == 78 && bt[0] == 0x31 && bt[12] == 0x05 && bt[23] == 0x21);
    stored_crc(bt);
}
