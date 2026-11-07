#include "mapping.hpp"
#include "config.hpp"
#include "log.hpp"
#include "profiles.hpp"
#include "test_mode.hpp"
#include "output_report.hpp"
#include "sink.hpp"
#include "source.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef DS2X_WINDOWS
#include "win_dynload.hpp"
#endif

using namespace ds2x;

static void usage() {
    std::cout << "dualshock2xbox ; play PC games with a DualShock 4 / DualSense\n"
              << "usage: dualshock2xbox [--dry-run] [--check] [--profile NAME] [--list-profiles]\n"
              << "                  [--hz N] [--deadzone F]\n"
              << "  --check          diagnose the setup and exit (driver, controller)\n"
              << "  --profile NAME   load profiles/NAME.ini instead of settings.ini\n"
              << "  --list-profiles  show available profiles and exit\n"
              << "  --safe           read-only mode ; never write anything to the pad\n"
              << "  --test-mode      keyboard->dualshock->xbox playground with a web GUI\n"
              << "  --port N         test-mode GUI port (default 8080)\n";
}

// plain-language health check for people who just want it to work
static int run_check() {
#ifdef DS2X_WINDOWS
    std::cout << "checking your setup...\n\n";
    if (!win::load()) {
        std::cout << "[!!] " << win::last_error() << "\n";
        return 1;
    }

    auto client = win::vigem_alloc();
    if (win::vigem_connect(client) == win::VIGEM_ERROR_NONE) {
        std::cout << "[ok] ViGEmBus driver found ; the fake Xbox pad can be created\n";
        win::vigem_disconnect(client);
    } else {
        std::cout << "[!!] ViGEmBus driver NOT found ; run setup.ps1 first\n";
    }
    win::vigem_free(client);

    win::hid_init();
    bool found = false;
    const std::uint16_t pids[] = {0x05C4, 0x09CC, 0x0CE6, 0x0DF2};
    for (std::uint16_t pid : pids) {
        win::hid_device_info* list = win::hid_enumerate(SONY_VID, pid);
        for (win::hid_device_info* it = list; it; it = it->next) {
            if (it->usage_page == 0x01 && it->usage == 0x05) {
                std::cout << "[ok] found a " << device_name(device_from_pid(pid))
                          << " controller\n";
                found = true;
            }
        }
        win::hid_free_enumeration(list);
    }
    if (!found)
        std::cout << "[!!] no DualShock/DualSense detected ; plug it in or pair it over Bluetooth\n";
    win::hid_exit();

    std::cout << "\nif both lines say [ok], just run dualshock2xbox.exe and play\n";
    return found ? 0 : 1;
#else
    std::cout << "--check needs Windows (that's where the driver and the pad live).\n"
              << "on this machine, try: ./dualshock2xbox --dry-run\n";
    return 0;
#endif
}

int main(int argc, char** argv) {
    [[maybe_unused]] bool dry_run = false;
    Config cfg;  // defaults; the chosen profile file overrides, flags override that
    std::string profile;
    bool list_only = false;
    bool test_mode = false;
    int port = 8080;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0 && i + 1 < argc) profile = argv[++i];
        else if (std::strcmp(argv[i], "--list-profiles") == 0) list_only = true;
        else if (std::strcmp(argv[i], "--test-mode") == 0) test_mode = true;
        else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = std::stoi(argv[++i]);
    }
    if (!load_profile(profile, cfg))
        log_info("no profile file found ; using defaults%s",
                 profile.empty() ? "" : (" (" + profile + ")").c_str());
    else if (!profile.empty())
        log_info("loaded profile '%s'", profile.c_str());
    if (list_only) {
        auto names = list_profiles();
        std::cout << "default profile: settings.ini\nprofiles in profiles/:\n";
        for (auto& n : names) std::cout << "  " << n << "\n";
        if (names.empty()) std::cout << "  (none yet ; copy settings.ini into profiles/)\n";
        return 0;
    }
    [[maybe_unused]] int hz = cfg.hz;
    float deadzone = cfg.deadzone;
    float gyro_gain = cfg.gyro ? cfg.gyro_gain : 0.0f;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dry-run") == 0) dry_run = true;
        else if (std::strcmp(argv[i], "--safe") == 0) cfg.safe_mode = true;
        else if (std::strcmp(argv[i], "--check") == 0) return run_check();
        else if (std::strcmp(argv[i], "--profile") == 0) { ++i; }  // handled above
        else if (std::strcmp(argv[i], "--list-profiles") == 0) {}   // handled above
        else if (std::strcmp(argv[i], "--test-mode") == 0) {}       // handled above
        else if (std::strcmp(argv[i], "--port") == 0) { ++i; }      // handled above
        else if (std::strcmp(argv[i], "--hz") == 0 && i + 1 < argc) hz = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--deadzone") == 0 && i + 1 < argc) deadzone = std::stof(argv[++i]);
        else if (std::strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else { usage(); return 1; }
    }

#ifdef DS2X_WINDOWS
    if (!dry_run) {
        if (!win::load()) {
            std::cerr << win::last_error() << "\n";
            return 1;
        }
        auto source = make_hid_source();
        if (!source->open()) {
            std::cerr << "no DualShock/DualSense found. paired and connected?\n"
                      << "(tip: run with --check to diagnose, or --dry-run without a pad)\n";
            return 1;
        }
        auto sink = make_vigem_sink();
        if (!sink->open()) return 1;

        // game rumbles the virtual pad ; forward it to the real one.
        // xinput large motor = ds strong motor, small = weak.
        // in safe mode the pad is never written to at all.
        std::uint8_t bt_seq = 0;
        if (cfg.safe_mode) {
            log_info("safe mode on ; the controller will only be read, never written");
        } else {
            sink->rumble_handler = [&](std::uint8_t large, std::uint8_t small) {
                source->write_output(rumble_report(source->device(), source->transport(),
                                                   small, large, bt_seq));
            };
        }

        if (cfg.lightbar && !cfg.safe_mode)
            source->write_output(lightbar_report(source->device(), source->transport(),
                                                 cfg.lr, cfg.lg, cfg.lb, bt_seq));
        if (cfg.player_led > 0 && !cfg.safe_mode && source->device() == Device::DualSense)
            source->write_output(player_led_report(source->transport(), cfg.player_led, bt_seq));
        if (cfg.trigger_mode > 0 && !cfg.safe_mode && source->device() == Device::DualSense) {
            TriggerEffect fx = trigger_off();
            switch (cfg.trigger_mode) {
                case 1: fx = trigger_feedback(cfg.trigger_start, cfg.trigger_strength); break;
                case 2: fx = trigger_weapon(cfg.trigger_start, cfg.trigger_start + 2, cfg.trigger_strength); break;
                case 3: fx = trigger_vibration(cfg.trigger_start, cfg.trigger_strength, cfg.trigger_freq); break;
                default: break;
            }
            source->write_output(dualsense_trigger_report(source->transport(), fx, fx, bt_seq));
        }

        const auto period = std::chrono::microseconds(1000000 / hz);
        while (true) {
            // pad unplugged or BT dropped? wait a second and look for it again
            if (!source->alive()) {
                std::cout << "controller lost ; waiting for it to come back...\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                while (!source->open())
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "controller back\n";
                continue;
            }
            if (auto report = source->read()) {
                if (auto pad = parse_report(source->device(), source->transport(),
                                            report->data(), report->size()))
                    sink->write(map_state(*pad, deadzone, gyro_gain, cfg.remap, cfg.anti_deadzone, cfg.max_zone));
            }
            std::this_thread::sleep_for(period);
        }
    }
#endif

    if (test_mode) {
        TestServer server;
        if (!server.start(port)) {
            std::cerr << "could not open port " << port << "\n";
            return 1;
        }
#ifdef DS2X_WINDOWS
        auto sink = dry_run ? make_logging_sink() : make_vigem_sink();
#else
        auto sink = make_logging_sink();
#endif
        if (!sink->open()) return 1;
        log_info("test mode GUI at http://localhost:%d", port);
        log_info("keyboard -> DualShock -> Xbox ; WASD/arrows/IJKL are sticks+dpad, 1-4 face buttons, q/e shoulders, z/x triggers");
        while (true) {
            server.poll();
            XboxState x = map_state(build_pad_state(server.keys()), deadzone, gyro_gain, cfg.remap, cfg.anti_deadzone, cfg.max_zone);
            sink->write(x);
            server.set_last_xbox(x);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    }

    // dry-run path (or non-Windows build): scripted pad -> parser -> mapper -> log
    std::cout << "dry-run mode: playing back a scripted DualSense session\n";
    auto source = make_scripted_source();
    auto sink = make_logging_sink();
    source->open();
    sink->open();
    if (cfg.lightbar) {
        std::uint8_t seq = 0;
        auto lb = lightbar_report(source->device(), source->transport(),
                                  cfg.lr, cfg.lg, cfg.lb, seq);
        std::cout << "[dry-run] light bar set to rgb(" << (int)cfg.lr << "," << (int)cfg.lg
                  << "," << (int)cfg.lb << ") ; " << lb.size() << "-byte report\n";
    }
    if (cfg.player_led > 0) {
        std::uint8_t seq = 0;
        auto pl = player_led_report(source->transport(), cfg.player_led, seq);
        std::cout << "[dry-run] player LED " << cfg.player_led << " ; "
                  << pl.size() << "-byte report\n";
    }
    {
        std::uint8_t seq = 0;
        auto tr = dualsense_trigger_report(source->transport(),
                                           trigger_feedback(2, 6), trigger_off(), seq);
        std::cout << "[dry-run] adaptive trigger demo (feedback on left) ; "
                  << tr.size() << "-byte report\n";
    }
    while (auto report = source->read()) {
        if (auto pad = parse_report(source->device(), source->transport(),
                                    report->data(), report->size()))
            sink->write(map_state(*pad, deadzone, gyro_gain, cfg.remap, cfg.anti_deadzone, cfg.max_zone));
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
    // fake a couple of game rumble pulses so the output path shows up too
    sink->rumble(255, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    sink->rumble(0, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    sink->rumble(0, 0);
    return 0;
}
