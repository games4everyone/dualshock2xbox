#include "log.hpp"

#include <cstdarg>

namespace ds2x {

void log_info(const char* fmt, ...) {
    std::fprintf(stdout, "[info] ");
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
    std::fputc('\n', stdout);
}

void log_warn(const char* fmt, ...) {
    std::fprintf(stderr, "[warn] ");
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

} // namespace ds2x
