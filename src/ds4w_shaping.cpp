#include "ds4w_shaping.hpp"

#include <cmath>

// Ported from DS4Windows' axis processing (GPL-3.0), simplified to the
// three knobs we expose: deadzone, anti-deadzone, max zone.
//   deadzone:     radial cut around center, output rescaled so the edge -> 0
//   anti-deadzone: out = adz + v*(1-adz) per axis, i.e. the smallest movement
//                  past the deadzone already produces `adz` output
//   max zone:     v / maxzone clamped to 1 per axis, so reaching `maxzone`
//                  physical deflection is enough for full output

namespace ds2x {

static float shape_axis(float v, float dz, float adz, float mz) {
    if (v <= dz && v >= -dz) return 0.0f;
    // rescale past the deadzone
    float sign = v < 0.0f ? -1.0f : 1.0f;
    float mag = (std::fabs(v) - dz) / (1.0f - dz);
    if (mag > 1.0f) mag = 1.0f;
    // anti-deadzone floor
    mag = adz + mag * (1.0f - adz);
    // max zone
    if (mz < 1.0f && mz > 0.0f) {
        mag /= mz;
        if (mag > 1.0f) mag = 1.0f;
    }
    return sign * mag;
}

void shape_stick(float& x, float& y, const StickShaping& s) {
    // radial deadzone first: if the vector is inside, zero both axes
    const float mag = std::hypot(x, y);
    if (mag <= s.deadzone) {
        x = 0.0f;
        y = 0.0f;
        return;
    }
    x = shape_axis(x, s.deadzone, s.anti_deadzone, s.max_zone);
    y = shape_axis(y, s.deadzone, s.anti_deadzone, s.max_zone);
}

} // namespace ds2x
