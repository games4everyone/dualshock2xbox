#include "profiles.hpp"

#include <algorithm>
#include <filesystem>

namespace ds2x {

std::vector<std::string> list_profiles(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return out;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension();
        if (ext == ".ini") out.push_back(entry.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string resolve_profile_path(const std::string& name, const std::string& dir) {
    if (name.empty()) return kDefaultProfile;
    // a name with .ini or a path separator is taken as-is ; bare names live in profiles/
    if (name.find('.') != std::string::npos || name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos)
        return name;
    return dir + "/" + name + ".ini";
}

bool load_profile(const std::string& name, Config& cfg) {
    return load_config(resolve_profile_path(name), cfg);
}

} // namespace ds2x
