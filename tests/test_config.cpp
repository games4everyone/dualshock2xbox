// config + profile tests

#include "test_util.hpp"

#include "config.hpp"
#include "profiles.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace ds2x;

void test_config() {
    const char* path = "test_settings_tmp.ini";
    {
        std::FILE* f = std::fopen(path, "w");
        std::fputs("# comment\n[section]\nhz=500\ndeadzone=0.2 ; inline comment\ngyro=1\nsafe_mode=1\n"
                   "gyro_gain=0.9\nlightbar=10,20,30\ntrigger_mode=1\nremap_cross=b\nbogus line\n", f);
        std::fclose(f);
    }
    Config cfg;
    CHECK(load_config(path, cfg));
    CHECK(cfg.hz == 500 && std::fabs(cfg.deadzone - 0.2f) < 1e-6);
    CHECK(cfg.gyro && std::fabs(cfg.gyro_gain - 0.9f) < 1e-6);
    CHECK(cfg.lightbar && cfg.lr == 10 && cfg.lg == 20 && cfg.lb == 30);
    CHECK(cfg.trigger_mode == 1);
    CHECK(cfg.safe_mode);
    CHECK(cfg.remap[ps_button_index("cross")] == XB);  // remapped
    Config def;
    CHECK(!load_config("definitely_not_here.ini", def));
    std::remove(path);
}

void test_profiles() {
    // bare name resolves into profiles/
    CHECK(resolve_profile_path("") == "settings.ini");
    CHECK(resolve_profile_path("racing") == "profiles/racing.ini");
    CHECK(resolve_profile_path("custom.ini") == "custom.ini");

    // listing a missing dir yields nothing, not a crash
    CHECK(list_profiles("no_such_dir_xyz").empty());

    // end to end: write a profile dir, list it, load it
    std::filesystem::create_directories("test_profiles_tmp");
    {
        std::FILE* f = std::fopen("test_profiles_tmp/fps.ini", "w");
        std::fputs("hz=500\ngyro=1\n", f);
        std::fclose(f);
    }
    auto names = list_profiles("test_profiles_tmp");
    CHECK(names.size() == 1 && names[0] == "fps");
    Config cfg;
    CHECK(load_profile("test_profiles_tmp/fps.ini", cfg));  // path form
    CHECK(cfg.hz == 500 && cfg.gyro);
    std::filesystem::remove_all("test_profiles_tmp");
}

// typos and nonsense must neither crash nor sneak past silently
void test_config_sanitize() {
    const char* path = "test_settings_bad_tmp.ini";
    {
        std::FILE* f = std::fopen(path, "wb");
        // starts with a UTF-8 BOM, like notepad likes to do
        std::fputs("\xEF\xBB\xBFhz=abc\ndeadzon=0.3\ndeadzone=7\nlightbar=300,-5,20\n"
                   "gyro_invert_yaw=1\nmax_zone=0\nplayer_led=9\ntrigger_start=-3\n", f);
        std::fclose(f);
    }
    Config cfg;
    CHECK(load_config(path, cfg));
    CHECK(cfg.hz == 250);                         // "abc" ignored, default kept, no crash
    CHECK(cfg.lightbar && cfg.lr == 255 && cfg.lg == 0 && cfg.lb == 20);  // clamped, not wrapped
    CHECK(cfg.gyro_invert_yaw && !cfg.gyro_invert_pitch);
    CHECK(std::fabs(cfg.deadzone - 7.0f) < 1e-6);  // the file said 7 ...
    int fixed = sanitize_config(cfg);
    CHECK(fixed == 3);                            // ... deadzone, max_zone, trigger_start
    CHECK(cfg.deadzone <= 0.95f && cfg.max_zone >= 0.1f);
    CHECK(cfg.player_led == 0 && cfg.trigger_start == 0);  // player_led=9 is refused at parse time

    Config zero;
    zero.hz = 0;
    CHECK(sanitize_config(zero) == 1 && zero.hz == 30);  // hz 0 would have been a division by zero
    Config fine;
    CHECK(sanitize_config(fine) == 0);             // sane defaults pass untouched
    std::remove(path);
}

void test_profile_lookup_next_to_exe() {
    namespace fs = std::filesystem;
    const fs::path dir = "test_exe_dir_tmp";
    fs::create_directories(dir / "profiles");
    {
        std::FILE* f = std::fopen((dir / "settings.ini").string().c_str(), "w");
        std::fputs("hz=333\n", f);
        std::fclose(f);
        f = std::fopen((dir / "profiles" / "fps.ini").string().c_str(), "w");
        std::fputs("hz=444\n", f);
        std::fclose(f);
    }
    // run from an empty directory (the repo root has its own settings.ini), so
    // only the exe-relative lookup can find these
    const fs::path old_cwd = fs::current_path();
    fs::create_directories("test_cwd_tmp");
    const fs::path abs_dir = fs::absolute(dir);
    fs::current_path("test_cwd_tmp");
    Config a;
    CHECK(!load_profile("", a));
    CHECK(load_profile("", a, abs_dir.string()) && a.hz == 333);
    Config b;
    CHECK(load_profile("fps", b, abs_dir.string()) && b.hz == 444);
    Config c;
    CHECK(!load_profile("nope", c, abs_dir.string()));
    fs::current_path(old_cwd);
    fs::remove_all("test_cwd_tmp");

    CHECK(exe_dir_from_argv0("C:/games/ds2x/dualshock2xbox.exe") == "C:/games/ds2x");
    CHECK(exe_dir_from_argv0("dualshock2xbox").empty());
    CHECK(exe_dir_from_argv0(nullptr).empty());
    fs::remove_all(dir);
}
