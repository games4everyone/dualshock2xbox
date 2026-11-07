#include "ds4w_shaping.hpp"

#include <cmath>

// Ported from DS4Windows' axis processing (GPL-3.0), simplified to the
// three knobs we expose: deadzone, anti-deadzone, max zone. (attribution: see
// NOTICE.txt). The math below has since been reworked to run on the stick's
// magnitude instead of per axis ; the knobs and their meaning are unchanged.
//
// The pipeline works on the stick's *magnitude* and keeps its direction:
//   deadzone:      magnitudes up to `deadzone` read as centered; the rest of the
//                  travel is rescaled so output starts smoothly at 0
//   max zone:      a worn stick that only reaches `max_zone` physical deflection
//                  still produces full output (the usable range is dz..max_zone)
//   anti-deadzone: lifts the floor so the smallest movement past the deadzone
//                  already produces `anti_deadzone` output, to clear the game's
//                  own deadzone
//
// Earlier versions shaped each axis on its own after a radial check, which
// zeroed the minor axis of a nearly-straight push (e.g. x=0.05, y=0.9 became
// pure vertical) and made aiming feel snappy along the axes. Working on the
// vector avoids that. Corners stay reachable: only each axis is clamped to
// +-1 at the end, the vector is not squeezed into a circle.

namespace ds2x {

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void shape_stick(float& x, float& y, const StickShaping& s) {
    // defend against nonsense settings instead of dividing by zero
    const float dz = clampf(s.deadzone, 0.0f, 0.95f);
    const float adz = clampf(s.anti_deadzone, 0.0f, 0.95f);
    float mz = clampf(s.max_zone, 0.05f, 1.0f);
    if (mz < dz + 0.05f) mz = clampf(dz + 0.05f, 0.05f, 1.0f);
    const float span = mz - dz;  // > 0 unless dz == 1 (excluded by the clamp above) and mz == 1

    const float mag = std::hypot(x, y);
    if (mag <= dz || mag <= 0.0f) {
        x = 0.0f;
        y = 0.0f;
        return;
    }

    float scaled = (mag - dz) / (span > 1e-6f ? span : 1e-6f);  // 0 at the deadzone edge, 1 at the max zone
    scaled = adz + scaled * (1.0f - adz);                       // anti-deadzone floor
    const float k = scaled / mag;                               // new magnitude / old magnitude
    x = clampf(x * k, -1.0f, 1.0f);
    y = clampf(y * k, -1.0f, 1.0f);
}

} // namespace ds2x
