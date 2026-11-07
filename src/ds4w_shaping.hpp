#pragma once

#include <cstdint>

namespace ds2x {

// Stick shaping pipeline ported from DS4Windows (https://github.com/Ryochan7/DS4Windows,
// GPL-3.0), specifically the axis-config behavior: inner deadzone, then
// anti-deadzone (raises the output floor so tiny inputs clear a game's own
// deadzone), then max-zone (a worn stick that only reaches e.g. 0.9 still
// hits full deflection).
//
// All values are fractions of full deflection: dz in [0,1], anti in [0,1),
// maxzone in (0,1] ; maxzone = 1.0 disables it.
//
// This file is a derivative work of DS4Windows and is GPL-3.0 like it.

struct StickShaping {
    float deadzone = 0.08f;
    float anti_deadzone = 0.0f;
    float max_zone = 1.0f;
};

// apply the pipeline to one stick axis pair (both in -1..+1, +Y up)
void shape_stick(float& x, float& y, const StickShaping& s);

} // namespace ds2x
