#pragma once

#include <cstdio>

namespace ds2x_test {
inline int& failures() {
    static int f = 0;
    return f;
}
} // namespace ds2x_test

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++ds2x_test::failures();                                       \
        }                                                                  \
    } while (0)
