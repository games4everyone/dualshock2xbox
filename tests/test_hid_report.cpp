// report parsing tests: DS4/DualSense, USB/BT, gyro, garbage, transport sniffing

#include "test_util.hpp"

#include "hid_report.hpp"

#include <vector>

using namespace ds2x;

static std::vector<std::uint8_t> ds4_report(std::uint8_t lx, std::uint8_t ly,
                                            std::uint8_t rx, std::uint8_t ry,
                                            std::uint8_t l2, std::uint8_t r2,
                                            std::uint8_t b1, std::uint8_t b2, std::uint8_t b3) {
    std::vector<std::uint8_t> r(64, 0);
    r[0] = 0x01;
    r[1] = lx; r[2] = ly; r[3] = rx; r[4] = ry;
    r[5] = b1; r[6] = b2; r[7] = b3;
    r[8] = l2; r[9] = r2;
    return r;
}

static std::vector<std::uint8_t> ds_report(std::uint8_t lx, std::uint8_t ly,
                                           std::uint8_t rx, std::uint8_t ry,
                                           std::uint8_t l2, std::uint8_t r2,
                                           std::uint8_t b1, std::uint8_t b2, std::uint8_t b3) {
    std::vector<std::uint8_t> r(64, 0);
    r[0] = 0x01;
    r[1] = lx; r[2] = ly; r[3] = rx; r[4] = ry;
    r[5] = l2; r[6] = r2;
    r[8] = b1; r[9] = b2; r[10] = b3;
    return r;
}

void test_ds4_usb() {
    auto r = ds4_report(255, 0, 128, 128, 200, 10, 0x08 | 0x20, 0x11, 0x03);
    auto s = parse_report(Device::DualShock4, Transport::Usb, r.data(), r.size());
    CHECK(s.has_value());
    CHECK(s->lx == 255 && s->ly == 0);
    CHECK(s->l2 == 200 && s->r2 == 10);
    CHECK(s->dpad == 8);
    CHECK(s->buttons & Cross);
    CHECK(s->buttons & L1);
    CHECK(s->buttons & Create);      // share normalized to create
    CHECK(s->buttons & PS && s->buttons & Touchpad);
    CHECK(!(s->buttons & Circle));
}

void test_ds4_bluetooth_full_report() {
    auto usb = ds4_report(10, 20, 30, 40, 50, 60, 0x00 | 0x40, 0x20, 0x00);
    std::vector<std::uint8_t> bt(66, 0);
    bt[0] = 0x11; bt[1] = 0xC0; bt[2] = 0x00;
    std::copy(usb.begin() + 1, usb.end(), bt.begin() + 3);
    auto s = parse_report(Device::DualShock4, Transport::Bluetooth, bt.data(), bt.size());
    CHECK(s.has_value());
    CHECK(s->lx == 10 && s->ly == 20 && s->rx == 30 && s->ry == 40);
    CHECK(s->l2 == 50 && s->r2 == 60);
    CHECK(s->dpad == 0);
    CHECK(s->buttons & Circle);
    CHECK(s->buttons & Options);
}

void test_dualsense_usb() {
    auto r = ds_report(128, 128, 200, 60, 255, 0, 0x02 | 0x80, 0x30, 0x01);
    auto s = parse_report(Device::DualSense, Transport::Usb, r.data(), r.size());
    CHECK(s.has_value());
    CHECK(s->rx == 200 && s->ry == 60);
    CHECK(s->l2 == 255 && s->r2 == 0);
    CHECK(s->dpad == 2);
    CHECK(s->buttons & Triangle && s->buttons & PS);
    CHECK(s->buttons & Create && s->buttons & Options);
}

void test_dualsense_bt_full_report() {
    auto usb = ds_report(1, 2, 3, 4, 5, 6, 0x08, 0x01, 0x02);
    std::vector<std::uint8_t> bt(65, 0);
    bt[0] = 0x31; bt[1] = 0x51;
    std::copy(usb.begin() + 1, usb.end(), bt.begin() + 2);
    auto s = parse_report(Device::DualSense, Transport::Bluetooth, bt.data(), bt.size());
    CHECK(s.has_value());
    CHECK(s->lx == 1 && s->ly == 2 && s->rx == 3 && s->ry == 4);
    CHECK(s->l2 == 5 && s->r2 == 6);
    CHECK(s->buttons & L1 && s->buttons & Touchpad);
}

void test_dualsense_bt_simple_is_ds4_shaped() {
    auto ds4shaped = ds4_report(90, 91, 92, 93, 94, 95, 0x04 | 0x20, 0x02, 0x01);
    auto s = parse_report(Device::DualSense, Transport::Bluetooth,
                          ds4shaped.data(), ds4shaped.size());
    CHECK(s.has_value());
    CHECK(s->lx == 90 && s->l2 == 94);
    CHECK(s->dpad == 4);
    CHECK(s->buttons & Cross && s->buttons & R1 && s->buttons & PS);
}

void test_garbage() {
    std::vector<std::uint8_t> junk = {0x99, 1, 2, 3};
    CHECK(!parse_report(Device::DualShock4, Transport::Usb, junk.data(), junk.size()));
    CHECK(!parse_report(Device::DualSense, Transport::Usb, junk.data(), junk.size()));
    std::vector<std::uint8_t> shorty = {0x01, 0x80};
    CHECK(!parse_report(Device::DualShock4, Transport::Usb, shorty.data(), shorty.size()));
    CHECK(!parse_report(Device::DualShock4, Transport::Usb, nullptr, 0));
}

void test_gyro_parse() {
    auto r = ds4_report(128, 128, 128, 128, 0, 0, 0x08, 0, 0);
    r[17] = 0xE8; r[18] = 0x03;  // gz = 1000
    auto s = parse_report(Device::DualShock4, Transport::Usb, r.data(), r.size());
    CHECK(s.has_value());
    CHECK(s->gz == 1000);

    auto r2 = ds_report(128, 128, 128, 128, 0, 0, 0x08, 0, 0);
    r2[20] = 0xE8; r2[21] = 0x03;
    auto s2 = parse_report(Device::DualSense, Transport::Usb, r2.data(), r2.size());
    CHECK(s2.has_value());
    CHECK(s2->gz == 1000);
}

void test_transport_from_path() {
    CHECK(transport_from_path("USB\\VID_054C&PID_0CE6\\5&1234") == Transport::Usb);
    CHECK(transport_from_path("BTH\\VID_054C&PID_0CE6\\7&abcd") == Transport::Bluetooth);
    CHECK(transport_from_path("\\\\?\\hid#bth&vid_054c") == Transport::Bluetooth);
    CHECK(transport_from_path(nullptr) == Transport::Usb);
    CHECK(transport_from_path("") == Transport::Usb);
}

// a wrong guess from the device path must not make a bluetooth pad look dead:
// the report id decides the layout
void test_dualsense_bt_ignores_wrong_transport_hint() {
    std::vector<std::uint8_t> bt(78, 0);
    bt[0] = 0x31; bt[1] = 0x01;
    bt[2] = 77; bt[3] = 88; bt[6] = 99;  // lx, ly, l2
    auto s = parse_report(Device::DualSense, Transport::Usb, bt.data(), bt.size());  // hint says USB
    CHECK(s.has_value() && s->lx == 77 && s->ly == 88 && s->l2 == 99);

    // and the 10-byte simple report is simple whatever the hint says
    std::vector<std::uint8_t> simple(10, 0);
    simple[0] = 0x01; simple[1] = 11; simple[2] = 22; simple[8] = 33;  // lx, ly, l2 (DS4 layout)
    auto t = parse_report(Device::DualSense, Transport::Usb, simple.data(), simple.size());
    CHECK(t.has_value() && t->lx == 11 && t->ly == 22 && t->l2 == 33);
}

void test_transport_from_report() {
    std::vector<std::uint8_t> r(78, 0);
    r[0] = 0x11; CHECK(transport_from_report(r.data(), r.size()) == Transport::Bluetooth);
    r[0] = 0x31; CHECK(transport_from_report(r.data(), r.size()) == Transport::Bluetooth);
    std::vector<std::uint8_t> usb(64, 0); usb[0] = 0x01;
    CHECK(transport_from_report(usb.data(), usb.size()) == Transport::Usb);
    std::vector<std::uint8_t> simple(10, 0); simple[0] = 0x01;
    CHECK(transport_from_report(simple.data(), simple.size()) == Transport::Bluetooth);
    std::vector<std::uint8_t> junk(4, 0); junk[0] = 0x7F;
    CHECK(!transport_from_report(junk.data(), junk.size()).has_value());
    CHECK(!transport_from_report(nullptr, 0).has_value());

    // real windows bluetooth HID paths carry the HID service uuid, not "BTH"
    CHECK(transport_from_path("\\\\?\\hid#{00001124-0000-1000-8000-00805f9b34fb}_vid&0002054c_pid&09cc#a&1b2c&0&0000#{4d1e55b2}") == Transport::Bluetooth);
    CHECK(transport_from_path("\\\\?\\hid#vid_054c&pid_09cc&col01#7&abc&0&0000#{4d1e55b2}") == Transport::Usb);
}
