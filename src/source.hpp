#pragma once

#include "hid_report.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace ds2x {

// a source yields raw HID reports from a controller
struct Source {
    virtual ~Source() = default;
    virtual bool open() = 0;  // find and open a pad; false if none found
    // next raw report (report id included at [0]), or nullopt if nothing new
    virtual std::optional<std::vector<std::uint8_t>> read() = 0;
    // false when the device is gone (unplugged / BT dropped) ; caller should re-open
    virtual bool alive() const { return true; }
    // send an output report (rumble etc.) back to the pad; false if unsupported
    virtual bool write_output(const std::vector<std::uint8_t>& report) { (void)report; return false; }
    // drain everything that is waiting and return only the newest report. at 250 Hz
    // a pad produces a report every 4 ms ; reading just one per loop turn lets the
    // OS queue back up and every input arrives later and later.
    virtual std::optional<std::vector<std::uint8_t>> read_latest() {
        std::optional<std::vector<std::uint8_t>> latest;
        // bounded, so a device that never runs dry can't hang the loop
        for (int i = 0; i < 64; ++i) {
            auto r = read();
            if (!r) break;
            latest = std::move(r);
        }
        return latest;
    }
    // bumps every time the pad is (re)opened or its transport turns out to differ
    // from the first guess ; the caller re-sends light bar / LED / trigger settings
    virtual unsigned generation() const { return 0; }
    virtual Device device() const = 0;
    virtual Transport transport() const = 0;
    virtual std::string name() const = 0;
};

#ifdef DS2X_WINDOWS
// real hardware via hidapi (Windows build)
std::unique_ptr<Source> make_hid_source();
#endif

// scripted source for --dry-run: plays back a little demo on any OS
std::unique_ptr<Source> make_scripted_source();

} // namespace ds2x
