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
