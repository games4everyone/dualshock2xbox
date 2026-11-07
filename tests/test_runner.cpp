// test runner: calls every module's tests, prints the summary

#include "test_util.hpp"

#include <cstdio>

void test_ds4_usb();
void test_ds4_bluetooth_full_report();
void test_dualsense_usb();
void test_dualsense_bt_full_report();
void test_dualsense_bt_simple_is_ds4_shaped();
void test_garbage();
void test_gyro_parse();
void test_transport_from_path();

void test_mapping_buttons();
void test_mapping_touchpad_ignored_by_default();
void test_mapping_axes();
void test_mapping_deadzone();
void test_mapping_triggers_and_dpad();
void test_gyro_mapping();
void test_remap();
void test_ds4w_shaping();

void test_crc32();
void test_rumble_reports();
void test_lightbar_reports();
void test_player_led_reports();
void test_trigger_reports();

void test_config();
void test_profiles();

void test_build_pad_state();
void test_server_handle();

int main() {
    test_ds4_usb();
    test_ds4_bluetooth_full_report();
    test_dualsense_usb();
    test_dualsense_bt_full_report();
    test_dualsense_bt_simple_is_ds4_shaped();
    test_garbage();
    test_gyro_parse();
    test_transport_from_path();

    test_mapping_buttons();
    test_mapping_touchpad_ignored_by_default();
    test_mapping_axes();
    test_mapping_deadzone();
    test_mapping_triggers_and_dpad();
    test_gyro_mapping();
    test_remap();
    test_ds4w_shaping();

    test_crc32();
    test_rumble_reports();
    test_lightbar_reports();
    test_player_led_reports();
    test_trigger_reports();

    test_config();
    test_profiles();

    test_build_pad_state();
    test_server_handle();

    if (ds2x_test::failures() == 0) std::printf("all tests passed\n");
    return ds2x_test::failures() == 0 ? 0 : 1;
}
