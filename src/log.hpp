#pragma once

#include <cstdio>

namespace ds2x {

// tiny logging so every module prints the same way ; [info] / [warn] prefixes
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);

} // namespace ds2x
