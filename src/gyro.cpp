#include "gyro.hpp"

#include <algorithm>

namespace ds2x {

GyroCalibrator::GyroCalibrator(std::size_t window, int still_range, int noise_floor)
    : window_(window < 8 ? 8 : window), still_range_(still_range), noise_floor_(noise_floor) {
    ring_.assign(window_, {0, 0, 0});
}

void GyroCalibrator::reset() {
    next_ = 0;
    count_ = 0;
    bias_ = {0, 0, 0};
    calibrated_ = false;
}

static std::int16_t clamp16(int v) {
    return static_cast<std::int16_t>(std::clamp(v, -32768, 32767));
}

void GyroCalibrator::apply(PadState& pad) {
    const std::array<int, 3> raw{pad.gx, pad.gy, pad.gz};

    ring_[next_] = raw;
    next_ = (next_ + 1) % window_;
    if (count_ < window_) ++count_;

    if (count_ == window_) {
        std::array<int, 3> lo = ring_[0], hi = ring_[0];
        std::array<long, 3> sum{0, 0, 0};
        for (const auto& s : ring_)
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], s[a]);
                hi[a] = std::max(hi[a], s[a]);
                sum[a] += s[a];
            }
        bool still = true;
        for (int a = 0; a < 3; ++a)
            if (hi[a] - lo[a] > still_range_) still = false;
        if (still) {
            for (int a = 0; a < 3; ++a) bias_[a] = static_cast<int>(sum[a] / static_cast<long>(window_));
            calibrated_ = true;
        }
    }

    auto fix = [&](int v, int b) {
        int d = v - b;
        if (d > -noise_floor_ && d < noise_floor_) d = 0;
        return clamp16(d);
    };
    pad.gx = fix(raw[0], bias_[0]);
    pad.gy = fix(raw[1], bias_[1]);
    pad.gz = fix(raw[2], bias_[2]);
}

} // namespace ds2x
