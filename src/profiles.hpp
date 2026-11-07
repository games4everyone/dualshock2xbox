#pragma once

#include "config.hpp"

#include <string>
#include <vector>

namespace ds2x {

// profiles are just ini files: settings.ini is the default, and any
// profiles/<name>.ini can be picked with --profile <name>
constexpr const char* kDefaultProfile = "settings.ini";
constexpr const char* kProfileDir = "profiles";

// all profile names found in profiles/ (without the .ini), sorted
std::vector<std::string> list_profiles(const std::string& dir = kProfileDir);

// resolve a profile name to a file path ; empty name means the default
std::string resolve_profile_path(const std::string& name,
                                 const std::string& dir = kProfileDir);

// load a profile by name into cfg ; missing file keeps defaults, returns false.
// the file is looked up relative to the current directory first, then relative
// to base_dir (pass the exe's folder: a Start Menu shortcut or "run as admin"
// often starts the program somewhere else, and settings.ini would silently
// not be found).
bool load_profile(const std::string& name, Config& cfg, const std::string& base_dir = "");

// the directory part of argv[0] (empty if it has none)
std::string exe_dir_from_argv0(const char* argv0);

} // namespace ds2x
