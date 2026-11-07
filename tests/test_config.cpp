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
