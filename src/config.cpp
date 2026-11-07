#include "config.hpp"

#include "log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace ds2x {

static std::string trim(std::string s) {
    auto notspace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

static bool parse_bool(const std::string& v) {
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

bool load_config(const std::string& path, Config& cfg) {
    std::ifstream f(path);
    if (!f) return false;  // no file ; defaults are fine

    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        // a UTF-8 BOM (notepad adds one) would glue itself to the first key
        if (lineno == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;  // comment or section header ; we don't need sections
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            log_warn("%s:%d: ignoring line without '=': %s", path.c_str(), lineno, line.c_str());
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // strip trailing inline comment
        auto hash = val.find_first_of("#;");
        if (hash != std::string::npos) val = trim(val.substr(0, hash));

        bool known = true;
        try {
            if (key == "hz") cfg.hz = std::stoi(val);
            else if (key == "deadzone") cfg.deadzone = std::stof(val);
            else if (key == "anti_deadzone") cfg.anti_deadzone = std::stof(val);
            else if (key == "max_zone") cfg.max_zone = std::stof(val);
            else if (key == "gyro") cfg.gyro = parse_bool(val);
            else if (key == "gyro_gain") cfg.gyro_gain = std::stof(val);
            else if (key == "gyro_invert_yaw") cfg.gyro_invert_yaw = parse_bool(val);
            else if (key == "gyro_invert_pitch") cfg.gyro_invert_pitch = parse_bool(val);
            else if (key == "lightbar") {
                // "lightbar=10,120,255" ; anything unparsable just keeps the default
                int r, g, b;
                if (std::sscanf(val.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
                    cfg.lightbar = true;
                    cfg.lr = static_cast<std::uint8_t>(std::clamp(r, 0, 255));
                    cfg.lg = static_cast<std::uint8_t>(std::clamp(g, 0, 255));
                    cfg.lb = static_cast<std::uint8_t>(std::clamp(b, 0, 255));
                } else {
                    log_warn("%s:%d: lightbar wants r,g,b (e.g. 0,80,255), got '%s'", path.c_str(), lineno, val.c_str());
                }
            }
            else if (key == "player_led") {
                int v = std::stoi(val);
                if (v >= 0 && v <= 5) cfg.player_led = v;
                else log_warn("%s:%d: player_led must be 0-5 (0 = off), got %d ; ignoring", path.c_str(), lineno, v);
            }
            else if (key == "safe_mode") cfg.safe_mode = parse_bool(val);
            else if (key == "trigger_mode") cfg.trigger_mode = std::stoi(val);
            else if (key == "trigger_start") cfg.trigger_start = std::stoi(val);
            else if (key == "trigger_strength") cfg.trigger_strength = std::stoi(val);
            else if (key == "trigger_freq") cfg.trigger_freq = std::stoi(val);
            else if (key.rfind("remap_", 0) == 0) {
                // remap_cross=b etc. ; bad names are ignored, defaults stay
                if (!parse_remap(key.substr(6), val, cfg.remap))
                    log_warn("%s:%d: can't remap '%s' to '%s' (unknown or non-remappable button)",
                             path.c_str(), lineno, key.substr(6).c_str(), val.c_str());
            }
            else if (key == "source") cfg.source_file = val;
            else known = false;
        } catch (...) {
            // a mangled line shouldn't kill the app ; keep the default, but say so
            log_warn("%s:%d: can't read the value of '%s' ('%s') ; keeping the default",
                     path.c_str(), lineno, key.c_str(), val.c_str());
        }
        if (!known) log_warn("%s:%d: unknown setting '%s' (typo?)", path.c_str(), lineno, key.c_str());
    }
    return true;
}

// clamp one value, logging when it had to move
template <typename T>
static int clamp_setting(const char* name, T& v, T lo, T hi) {
    if (v >= lo && v <= hi) return 0;  // NaN fails both comparisons and lands below
    const T fixed = (v != v) ? lo : (v < lo ? lo : hi);
    log_warn("setting '%s' = %g is out of range [%g, %g] ; using %g", name,
             static_cast<double>(v), static_cast<double>(lo), static_cast<double>(hi),
             static_cast<double>(fixed));
    v = fixed;
    return 1;
}

int sanitize_config(Config& cfg) {
    int n = 0;
    n += clamp_setting("hz", cfg.hz, 30, 1000);
    n += clamp_setting("deadzone", cfg.deadzone, 0.0f, 0.95f);
    n += clamp_setting("anti_deadzone", cfg.anti_deadzone, 0.0f, 0.95f);
    n += clamp_setting("max_zone", cfg.max_zone, 0.1f, 1.0f);
    n += clamp_setting("gyro_gain", cfg.gyro_gain, 0.0f, 10.0f);
    n += clamp_setting("player_led", cfg.player_led, 0, 5);
    n += clamp_setting("trigger_mode", cfg.trigger_mode, 0, 3);
    n += clamp_setting("trigger_start", cfg.trigger_start, 0, 9);
    n += clamp_setting("trigger_strength", cfg.trigger_strength, 0, 8);
    n += clamp_setting("trigger_freq", cfg.trigger_freq, 0, 255);
    return n;
}

} // namespace ds2x
