#include "source.hpp"

#include <chrono>
#include <cstring>
#include <iostream>

#ifdef DS2X_WINDOWS
#include "win_dynload.hpp"
using namespace ds2x::win;  // hid_* calls resolve to the loaded function pointers
#endif

namespace ds2x {

#ifdef DS2X_WINDOWS

class HidSource : public Source {
public:
    ~HidSource() override { close(); }

    bool open() override {
        close();
        hid_init();
        // walk Sony's product ids; first pad that answers wins
        const std::uint16_t pids[] = {0x05C4, 0x09CC, 0x0CE6, 0x0DF2};
        for (std::uint16_t pid : pids) {
            hid_device_info* list = hid_enumerate(SONY_VID, pid);
            for (hid_device_info* it = list; it; it = it->next) {
                // skip non-gamepad interfaces (the DS4 also exposes an audio one)
                if (it->usage_page == 0x01 && it->usage == 0x05) {
                    dev_ = hid_open_path(it->path);
                    if (dev_) {
                        hid_set_nonblocking(dev_, 1);
                        pid_ = pid;
                        transport_ = transport_from_path(it->path);  // a guess ; read() corrects it
                        ++generation_;
                        if (transport_ == Transport::Bluetooth)
                            switch_to_full_report();
                        hid_free_enumeration(list);
                        std::cout << "opened " << device_name(device())
                                  << (transport_ == Transport::Bluetooth ? " over bluetooth" : " over usb")
                                  << " (pid 0x" << std::hex << pid << std::dec << ")\n";
                        return true;
                    }
                }
            }
            hid_free_enumeration(list);
        }
        return false;
    }

    std::optional<std::vector<std::uint8_t>> read() override {
        if (!dev_) return std::nullopt;
        // bluetooth full reports are 78 bytes ; a 64-byte buffer would truncate them
        std::uint8_t buf[128];
        int n = hid_read(dev_, buf, sizeof(buf));
        if (n < 0) { alive_ = false; return std::nullopt; }  // device gone
        if (n == 0) return std::nullopt;  // non-blocking: nothing new
        observe_transport(buf, static_cast<std::size_t>(n));
        return std::vector<std::uint8_t>(buf, buf + n);
    }

    bool alive() const override { return alive_; }
    unsigned generation() const override { return generation_; }

    bool write_output(const std::vector<std::uint8_t>& report) override {
        if (!dev_) return false;
        // note: hid_write wants the report id in [0], which our builders keep.
         // a write returning 0 means nothing for these pads ; they ack and ignore.
        return hid_write(dev_, report.data(), report.size()) == static_cast<int>(report.size());
    }

    Device device() const override { return device_from_pid(pid_); }
    Transport transport() const override { return transport_; }
    std::string name() const override { return name_; }

private:
    // over bluetooth both pads start in a stripped-down "simple" report (0x01).
    // reading a calibration feature report flips them to the full report
    // (DS4: 0x11, DualSense: 0x31) with gyro and everything else. this is the
    // part every tutorial skips, and without it BT pads look half dead.
    //
    // which feature report does the trick differs between pads and firmware, and
    // asking for one a pad doesn't have is harmless, so ask for the usual
    // suspects: calibration (DS4 0x02 / BT-only 0x05, DualSense 0x05) and, for the
    // DualSense, firmware info (0x20). (not verified on every firmware: if a
    // pad stays in simple mode over BT, this is the place to look.)
    void switch_to_full_report() {
        const std::uint8_t ds4_ids[] = {0x02, 0x05};
        const std::uint8_t ds5_ids[] = {0x05, 0x20};
        const bool ds4 = device() == Device::DualShock4;
        const std::uint8_t* ids = ds4 ? ds4_ids : ds5_ids;
        for (int i = 0; i < 2; ++i) {
            std::uint8_t buf[128] = {};
            buf[0] = ids[i];
            (void)hid_get_feature_report(dev_, buf, sizeof(buf));  // the read is the trigger ; result unused
        }
    }

    // the device path is only a hint about the transport ; the first report
    // is the proof (0x11 / 0x31 exist only over BT, a 64-byte 0x01 is USB)
    void observe_transport(const std::uint8_t* data, std::size_t len) {
        auto t = transport_from_report(data, len);
        if (!t || *t == transport_) return;
        transport_ = *t;
        ++generation_;
        std::cout << "this pad is actually on " << (transport_ == Transport::Bluetooth ? "bluetooth" : "usb")
                  << " ; adjusting\n";
        if (transport_ == Transport::Bluetooth) switch_to_full_report();
    }

    void close() {
        if (dev_) { hid_close(dev_); dev_ = nullptr; }
        alive_ = true;
        hid_exit();
    }
    hid_device* dev_ = nullptr;
    std::uint16_t pid_ = 0;
    Transport transport_ = Transport::Usb;
    bool alive_ = true;
    unsigned generation_ = 0;
    std::string name_ = "pad";
};

std::unique_ptr<Source> make_hid_source() { return std::make_unique<HidSource>(); }

#endif // DS2X_WINDOWS

// --- scripted source --------------------------------------------------------
// plays back a tiny scripted DualSense-over-USB session so --dry-run shows the
// full pipeline (parse -> map -> print) with zero hardware. each entry is one
// raw report; a 0 report id means "pause a beat".
class ScriptedSource : public Source {
public:
    bool open() override {
        // build a few DualSense USB reports (report id 0x01) by hand
        auto report = [](std::uint8_t lx, std::uint8_t ly, std::uint8_t rx, std::uint8_t ry,
                         std::uint8_t l2, std::uint8_t r2, std::uint8_t b1, std::uint8_t b2,
                         std::uint8_t b3) {
            std::vector<std::uint8_t> r(64, 0);
            r[0] = 0x01;
            r[1] = lx; r[2] = ly; r[3] = rx; r[4] = ry;
            r[5] = l2; r[6] = r2;
            r[8] = b1; r[9] = b2; r[10] = b3;
            return r;
        };
        const std::uint8_t C = 128;  // stick center
        frames_ = {
            report(C, C, C, C, 0, 0, 0x08, 0, 0),                    // idle
            report(C, C, C, C, 0, 0, 0x08 | 0x20, 0, 0),             // cross
            report(C, C, C, C, 0, 0, 0x08, 0, 0),                    // release
            report(255, C, C, C, 0, 0, 0x08, 0, 0),                  // stick full right
            report(C, 0, C, C, 0, 0, 0x08, 0, 0),                    // stick full up
            report(C, C, C, C, 0, 255, 0x08, 0x08, 0),               // R2 + R2-button
            report(C, C, C, C, 0, 0, 0x08, 0x20, 0x01),              // options + ps
            report(C, C, C, C, 0, 0, 0x00, 0, 0),                    // dpad north
            report(C, C, C, C, 0, 0, 0x08, 0, 0),                    // back to idle
        };
        idx_ = 0;
        return true;
    }

    std::optional<std::vector<std::uint8_t>> read() override {
        if (idx_ >= frames_.size()) return std::nullopt;
        return frames_[idx_++];
    }

    Device device() const override { return Device::DualSense; }
    Transport transport() const override { return Transport::Usb; }
    std::string name() const override { return "scripted DualSense"; }

private:
    std::vector<std::vector<std::uint8_t>> frames_;
    std::size_t idx_ = 0;
};

std::unique_ptr<Source> make_scripted_source() { return std::make_unique<ScriptedSource>(); }

} // namespace ds2x
