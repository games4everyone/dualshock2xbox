#include "mapping.hpp"
#include "config.hpp"
#include "gyro.hpp"
#include "log.hpp"
#include "profiles.hpp"
#include "test_mode.hpp"
#include "output_report.hpp"
#include "sink.hpp"
#include "source.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

#ifdef DS2X_WINDOWS
#include "win_dynload.hpp"
#endif

using namespace ds2x;

// ctrl+c, window close, logoff ; all roads lead to this flag, and every loop below
// watches it so the pad gets left calm and the virtual one gets removed properly
static std::atomic<bool> g_quit{false};
static void on_signal(int) { g_quit.store(true); }

static void usage() {
    std::cout << "dualshock2xbox ; play PC games with a DualShock 4 / DualSense\n"
              << "usage: dualshock2xbox [--dry-run] [--check] [--profile NAME] [--list-profiles]\n"
              << "                  [--hz N] [--deadzone F] [--safe] [--test-mode] [--port N]\n"
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

// ---- command line ------------------------------------------------------------
struct Args {
    bool dry_run = false;
    bool safe = false;
    bool check = false;
    bool list_only = false;
    bool test_mode = false;
    bool help = false;
    std::string profile;
    int port = 8080;
    std::optional<int> hz;
    std::optional<float> deadzone;
};

// std::stoi("abc") throws and std::stoi("12abc") happily returns 12 ; neither is
// what anyone meant, and an uncaught throw is a crash dialog for a typo
static bool to_int(const char* flag, const char* text, int& out) {
    try {
        std::size_t used = 0;
        const int v = std::stoi(text, &used);
        if (text[used] != '\0') throw std::invalid_argument("trailing junk");
        out = v;
        return true;
    } catch (...) {
        std::cerr << flag << " wants a whole number, got '" << text << "'\n";
        return false;
    }
}

static bool to_float(const char* flag, const char* text, float& out) {
    try {
        std::size_t used = 0;
        const float v = std::stof(text, &used);
        if (text[used] != '\0') throw std::invalid_argument("trailing junk");
        out = v;
        return true;
    } catch (...) {
        std::cerr << flag << " wants a number, got '" << text << "'\n";
        return false;
    }
}

// false (after saying why) when the command line makes no sense
static bool parse_args(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        // a flag that needs a value but is the last word is a mistake, not a default
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs a value\n";
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(arg, "--dry-run") == 0) a.dry_run = true;
        else if (std::strcmp(arg, "--safe") == 0) a.safe = true;
        else if (std::strcmp(arg, "--check") == 0) a.check = true;
        else if (std::strcmp(arg, "--list-profiles") == 0) a.list_only = true;
        else if (std::strcmp(arg, "--test-mode") == 0) a.test_mode = true;
        else if (std::strcmp(arg, "--help") == 0) a.help = true;
        else if (std::strcmp(arg, "--profile") == 0) {
            const char* v = value();
            if (!v) return false;
            a.profile = v;
        } else if (std::strcmp(arg, "--port") == 0) {
            const char* v = value();
            int p = 0;
            if (!v || !to_int(arg, v, p)) return false;
            if (p < 1 || p > 65535) {
                std::cerr << "--port must be between 1 and 65535\n";
                return false;
            }
            a.port = p;
        } else if (std::strcmp(arg, "--hz") == 0) {
            const char* v = value();
            int n = 0;
            if (!v || !to_int(arg, v, n)) return false;
            a.hz = n;
        } else if (std::strcmp(arg, "--deadzone") == 0) {
            const char* v = value();
            float f = 0;
            if (!v || !to_float(arg, v, f)) return false;
            a.deadzone = f;
        } else {
            std::cerr << "unknown option '" << arg << "'\n";
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        usage();
        return 1;
    }
    if (args.help) {
        usage();
        return 0;
    }
    if (args.check) return run_check();

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
#ifdef DS2X_WINDOWS
    win::install_quit_handler(&g_quit);
#endif

    [[maybe_unused]] const bool dry_run = args.dry_run;
    const bool test_mode = args.test_mode;
    const int port = args.port;

    const std::string base_dir = exe_dir_from_argv0(argv[0]);
    Config cfg;  // defaults; the chosen profile file overrides, flags override that
    const bool loaded = load_profile(args.profile, cfg, base_dir);

    if (args.list_only) {
        auto names = list_profiles();
        if (names.empty() && !base_dir.empty()) names = list_profiles(base_dir + "/" + kProfileDir);
        std::cout << "default profile: settings.ini\nprofiles in profiles/:\n";
        for (auto& n : names) std::cout << "  " << n << "\n";
        if (names.empty()) std::cout << "  (none yet ; copy settings.ini into profiles/)\n";
        return 0;
    }

    if (!args.profile.empty() && !loaded) {
        // a typo in --profile used to fall back to the defaults without a word, which
        // is a great way to spend an evening wondering why your deadzone does nothing
        std::cerr << "profile '" << args.profile << "' not found ; try --list-profiles\n";
        return 1;
    }
    if (!loaded)
        log_info("no profile file found ; using defaults");
    else if (!args.profile.empty())
        log_info("loaded profile '%s'", args.profile.c_str());

    // flags beat the profile ; then everything gets range-checked, because
    // "--hz 0" is a division by zero and "--deadzone 1" is a stick made of concrete
    if (args.hz) cfg.hz = *args.hz;
    if (args.deadzone) cfg.deadzone = *args.deadzone;
    if (args.safe) cfg.safe_mode = true;
    sanitize_config(cfg);

    const float deadzone = cfg.deadzone;
    const float gyro_gain = cfg.gyro ? cfg.gyro_gain : 0.0f;
    // one place builds the Xbox state so every path (real, test, dry-run) agrees
    auto translate = [&](const PadState& pad) {
        return map_state(pad, deadzone, gyro_gain, cfg.remap, cfg.anti_deadzone, cfg.max_zone,
                         cfg.gyro_invert_yaw, cfg.gyro_invert_pitch);
    };

#ifdef DS2X_WINDOWS
    // --test-mode is the "no controller needed" playground ; it must not fall into
    // the real-pad loop below (which would then insist on finding a controller)
    if (!dry_run && !test_mode) {
        if (!win::load()) {
            std::cerr << win::last_error() << "\n";
            return 1;
        }
        // windows naps in ~15 ms steps unless asked nicely, and 15 ms is 4 reports
        win::begin_high_res_timer();
        auto source = make_hid_source();
        if (!source->open()) {
            std::cerr << "no DualShock/DualSense found. paired and connected?\n"
                      << "(tip: run with --check to diagnose, or --dry-run without a pad)\n";
            return 1;
        }
        auto sink = make_vigem_sink();
        if (!sink->open()) return 1;

        std::uint8_t bt_seq = 0;

        // light bar, player LEDs, adaptive triggers ; sent again whenever the pad is
        // (re)opened or turns out to be on a different transport than first guessed,
        // so a reconnect doesn't leave you with a stock blue light bar
        auto apply_output_settings = [&]() {
            if (cfg.safe_mode) return;
            if (cfg.lightbar)
                source->write_output(lightbar_report(source->device(), source->transport(),
                                                     cfg.lr, cfg.lg, cfg.lb, bt_seq));
            if (cfg.player_led > 0 && source->device() == Device::DualSense)
                source->write_output(player_led_report(source->transport(), cfg.player_led, bt_seq));
            if (cfg.trigger_mode > 0 && source->device() == Device::DualSense) {
                TriggerEffect fx = trigger_off();
                switch (cfg.trigger_mode) {
                    case 1: fx = trigger_feedback(cfg.trigger_start, cfg.trigger_strength); break;
                    case 2: fx = trigger_weapon(cfg.trigger_start, cfg.trigger_start + 2, cfg.trigger_strength); break;
                    case 3: fx = trigger_vibration(cfg.trigger_start, cfg.trigger_strength, cfg.trigger_freq); break;
                    default: break;
                }
                source->write_output(dualsense_trigger_report(source->transport(), fx, fx, bt_seq));
            }
        };

        // game rumbles the virtual pad ; forward it to the real one.
        // xinput large motor = ds strong motor, small = weak.
        // in safe mode the pad is never written to at all.
        // the notification arrives on a ViGEm thread, so it only drops the values
        // here ; the loop below does the actual writing (one thread talks to the
        // pad, the output sequence counter stays sane, nobody has to learn locking)
        std::atomic<std::uint16_t> pending_rumble{0};  // (large << 8) | small
        std::atomic<bool> rumble_dirty{false};
        if (cfg.safe_mode) {
            log_info("safe mode on ; the controller will only be read, never written");
        } else {
            sink->rumble_handler = [&](std::uint8_t large, std::uint8_t small) {
                pending_rumble.store(static_cast<std::uint16_t>((large << 8) | small));
                rumble_dirty.store(true);
            };
        }

        GyroCalibrator gyro_cal;
        unsigned applied_generation = ~0u;
        int unparsed_in_a_row = 0;
        bool warned_unparsed = false;

        const auto period = std::chrono::microseconds(1000000 / cfg.hz);
        auto next_tick = std::chrono::steady_clock::now();
        while (!g_quit.load()) {
            // pad unplugged or BT dropped? wait a second and look for it again
            if (!source->alive()) {
                std::cout << "controller lost ; waiting for it to come back...\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                while (!g_quit.load() && !source->open())
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                if (g_quit.load()) break;
                std::cout << "controller back\n";
                gyro_cal.reset();  // a different pad (or a different table) has a different bias
                next_tick = std::chrono::steady_clock::now();
                continue;
            }

            // drain the queue and keep only the newest report. one report per turn
            // is how you end up playing 130 ms in the past without knowing why
            if (auto report = source->read_latest()) {
                if (auto pad = parse_report(source->device(), source->transport(),
                                            report->data(), report->size())) {
                    unparsed_in_a_row = 0;
                    if (source->generation() != applied_generation) {
                        applied_generation = source->generation();
                        apply_output_settings();
                    }
                    if (gyro_gain > 0.0f) gyro_cal.apply(*pad);
                    sink->write(translate(*pad));
                } else if (!warned_unparsed && ++unparsed_in_a_row > cfg.hz * 2) {
                    // the pad talks, we don't understand ; say so once instead of
                    // leaving you staring at a perfectly dead virtual controller
                    warned_unparsed = true;
                    log_warn("the controller keeps sending reports I can't read (first byte 0x%02x) ;"
                             " try plugging it in with a cable, and tell me which model/firmware it is",
                             report->empty() ? 0 : (*report)[0]);
                }
            }

            if (rumble_dirty.exchange(false) && !cfg.safe_mode) {
                const std::uint16_t v = pending_rumble.load();
                const auto large = static_cast<std::uint8_t>(v >> 8);
                const auto small = static_cast<std::uint8_t>(v & 0xFF);
                source->write_output(rumble_report(source->device(), source->transport(),
                                                   small, large, bt_seq));
            }

            // steady ticks instead of "work + sleep", so the rate doesn't sag by
            // however long the work took ; if we ever fall far behind (laptop lid,
            // debugger) just restart the clock instead of sprinting to catch up
            next_tick += period;
            const auto now = std::chrono::steady_clock::now();
            if (next_tick < now - std::chrono::milliseconds(100)) next_tick = now;
            std::this_thread::sleep_until(next_tick);
        }

        // leave the pad calm: a rumble motor left running after we exit is an
        // effective way to drain a battery and annoy a desk
        if (!cfg.safe_mode && source->alive())
            source->write_output(rumble_report(source->device(), source->transport(), 0, 0, bt_seq));
        sink->close();
        win::end_high_res_timer();
        std::cout << "bye\n";
        return 0;
    }
#endif

    if (test_mode) {
        TestServer server;
        if (!server.start(port)) {
            std::cerr << "could not open port " << port << "\n";
            return 1;
        }
#ifdef DS2X_WINDOWS
        win::begin_high_res_timer();
        std::unique_ptr<Sink> sink;
        if (dry_run) {
            sink = make_logging_sink();
        } else if (win::load()) {
            sink = make_vigem_sink();
        } else {
            // the playground still works without the driver ; it just can't make a real pad
            log_warn("%s ; showing the virtual pad in the console instead", win::last_error().c_str());
            sink = make_logging_sink();
        }
#else
        auto sink = make_logging_sink();
#endif
        if (!sink->open()) {
            log_warn("could not create the virtual pad (is ViGEmBus installed? try --check) ;"
                     " the GUI still works, states just go to the console");
            sink = make_logging_sink();
            sink->open();
        }
        log_info("test mode GUI at http://localhost:%d", port);
        log_info("keyboard -> DualShock -> Xbox ; WASD/arrows/IJKL are sticks+dpad, 1-4 face buttons, q/e shoulders, z/x triggers");
        while (!g_quit.load()) {
            server.poll();
            XboxState x = translate(build_pad_state(server.keys()));
            sink->write(x);
            server.set_last_xbox(x);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        sink->close();
#ifdef DS2X_WINDOWS
        win::end_high_res_timer();
#endif
        return 0;
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
    while (!g_quit.load()) {
        auto report = source->read();
        if (!report) break;
        if (auto pad = parse_report(source->device(), source->transport(),
                                    report->data(), report->size()))
            sink->write(translate(*pad));
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
