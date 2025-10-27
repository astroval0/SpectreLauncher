#pragma once

#include "common.h"
#include <vector>

// tries a bunch of different registry keys to find steam install path
[[nodiscard]] op get_steam_path();

// checks the windows uninstall registry for steam apps
[[nodiscard]] op get_app_install_from_uninstall(int id);

// searches all steam libraries for the game by parsing manifest files
[[nodiscard]] op get_app_install_by_manifests(const wstr& steam_path, int id);

// tries to get the current logged in steam user's steamid64
[[nodiscard]] op get_current_steamid64(const wstr& steam_path);

// reads libraryfolders.vdf to find all steam library locations
[[nodiscard]] std::vector<wstr> get_library_roots(const wstr& steam_path);

// parses steam manifest files (.acf) to get the install directory name
[[nodiscard]] std::optional<std::string> get_install_dir_from_manifest(const fs::path& manifest);