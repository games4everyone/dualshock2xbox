#pragma once

#include "hid_report.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace ds2x {

// Gyro sensors never read exactly zero at rest: every pad has a small constant
// offset (the "bias"), a quiet conviction that the desk is slowly rotating. we
// disagree with the desk. added to the right stick that offset makes the camera
// creep by itself. GyroCalibrator watches the raw samples, and whenever the pad has
// been held still for a whole window it re-estimates the bias as the window's
// average. Real motion (a window with a wide range) never updates it, so aiming
// can't be "calibrated away".
class GyroCalibrator {
public:
    // window: samples that must all be quiet (100 = 0.4 s at 250 Hz)
    // still_range: max (max - min) per axis, in raw units, that counts as still
    // noise_floor: after bias removal, |value| below this is treated as 0
    explicit GyroCalibrator(std::size_t window = 100, int still_range = 80, int noise_floor = 12);

    // update the estimate from this sample, then subtract the bias from
    // pad.gx / gy / gz in place
    void apply(PadState& pad);

    bool calibrated() const { return calibrated_; }
    int bias(int axis) const { return bias_[axis < 0 ? 0 : (axis > 2 ? 2 : axis)]; }
    void reset();

private:
    std::size_t window_;
    int still_range_;
    int noise_floor_;
    std::vector<std::array<int, 3>> ring_;
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    std::array<int, 3> bias_{0, 0, 0};
    bool calibrated_ = false;
};

} // namespace ds2x
