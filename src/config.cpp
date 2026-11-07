#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ds2x {

static std::string trim(std::string s) {
    auto notspace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

bool load_config(const std::string& path, Config& cfg) {
    std::ifstream f(path);
    if (!f) return false;  // no file ; defaults are fine

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;  // comment or section header ; we don't need sections
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // strip trailing inline comment
        auto hash = val.find_first_of("#;");
        if (hash != std::string::npos) val = trim(val.substr(0, hash));

        try {
            if (key == "hz") cfg.hz = std::stoi(val);
            else if (key == "deadzone") cfg.deadzone = std::stof(val);
            else if (key == "anti_deadzone") cfg.anti_deadzone = std::stof(val);
            else if (key == "max_zone") cfg.max_zone = std::stof(val);
            else if (key == "gyro") cfg.gyro = (val == "1" || val == "true" || val == "on");
            else if (key == "gyro_gain") cfg.gyro_gain = std::stof(val);
            else if (key == "lightbar") {
                // "lightbar=10,120,255" ; anything unparsable just keeps the default
                int r, g, b;
                if (std::sscanf(val.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
                    cfg.lightbar = true;
                    cfg.lr = static_cast<std::uint8_t>(r);
                    cfg.lg = static_cast<std::uint8_t>(g);
                    cfg.lb = static_cast<std::uint8_t>(b);
                }
            }
            else if (key == "player_led") {
                int v = std::stoi(val);
                if (v >= 0 && v <= 5) cfg.player_led = v;
            }
            else if (key == "safe_mode") cfg.safe_mode = (val == "1" || val == "true" || val == "on");
            else if (key == "trigger_mode") cfg.trigger_mode = std::stoi(val);
            else if (key == "trigger_start") cfg.trigger_start = std::stoi(val);
            else if (key == "trigger_strength") cfg.trigger_strength = std::stoi(val);
            else if (key == "trigger_freq") cfg.trigger_freq = std::stoi(val);
            else if (key.rfind("remap_", 0) == 0) {
                // remap_cross=b etc. ; bad names are ignored, defaults stay
                parse_remap(key.substr(6), val, cfg.remap);
            }
            else if (key == "source") cfg.source_file = val;
        } catch (...) {
            // a mangled line shouldn't kill the app ; keep the default
        }
    }
    return true;
}

} // namespace ds2x
