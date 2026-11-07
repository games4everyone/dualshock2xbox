// gyro bias calibration tests

#include "test_util.hpp"

#include "gyro.hpp"

#include <cstdlib>

using namespace ds2x;

static PadState sample(int gx, int gy, int gz) {
    PadState p{Device::DualSense};
    p.gx = static_cast<std::int16_t>(gx);
    p.gy = static_cast<std::int16_t>(gy);
    p.gz = static_cast<std::int16_t>(gz);
    return p;
}

void test_gyro_calibration() {
    // a pad lying on the desk: constant offset plus a little jitter. after a window
    // of that the output must settle at zero instead of creeping the camera
    GyroCalibrator cal(100, 80, 12);
    std::srand(7);
    PadState last;
    for (int i = 0; i < 300; ++i) {
        last = sample(40 + std::rand() % 9 - 4, -25 + std::rand() % 9 - 4, 10 + std::rand() % 9 - 4);
        cal.apply(last);
    }
    CHECK(cal.calibrated());
    CHECK(std::abs(cal.bias(0) - 40) <= 4 && std::abs(cal.bias(1) + 25) <= 4 && std::abs(cal.bias(2) - 10) <= 4);
    CHECK(last.gx == 0 && last.gy == 0 && last.gz == 0);

    // real motion passes through, minus the bias
    PadState turn = sample(40, 2000 - 25, 10);
    cal.apply(turn);
    CHECK(std::abs(turn.gy - 2000) <= 4);
    CHECK(turn.gx == 0 && turn.gz == 0);
}

void test_gyro_motion_is_not_calibrated_away() {
    // someone swinging the pad around constantly must never become "the bias"
    GyroCalibrator cal(100, 80, 12);
    for (int i = 0; i < 1000; ++i) {
        PadState p = sample((i % 2) ? 3000 : -3000, (i % 3) ? 1500 : -1500, 0);
        cal.apply(p);
    }
    CHECK(!cal.calibrated());
    CHECK(cal.bias(0) == 0 && cal.bias(1) == 0 && cal.bias(2) == 0);

    // a window that is still but not quiet enough (> still_range) also doesn't count
    GyroCalibrator loose(100, 80, 12);
    for (int i = 0; i < 400; ++i) {
        PadState p = sample((i % 2) ? 300 : 0, 0, 0);  // range 300 > 80
        loose.apply(p);
    }
    CHECK(!loose.calibrated());
}

void test_gyro_recalibrates_and_resets() {
    GyroCalibrator cal(50, 80, 12);
    for (int i = 0; i < 100; ++i) { PadState p = sample(100, 0, 0); cal.apply(p); }
    CHECK(cal.calibrated() && cal.bias(0) == 100);
    // the pad warms up and the offset drifts ; the estimate follows it
    for (int i = 0; i < 100; ++i) { PadState p = sample(130, 0, 0); cal.apply(p); }
    CHECK(cal.bias(0) == 130);
    cal.reset();
    CHECK(!cal.calibrated() && cal.bias(0) == 0);

    // extremes don't wrap around int16
    PadState big = sample(32767, -32768, 0);
    cal.apply(big);
    CHECK(big.gx == 32767 && big.gy == -32768);
}
