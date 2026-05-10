#pragma once

#include "common.h"
#include <vector>

// tries a bunch of different registry keys to find steam install path
[[nodiscard]] std::optional<std::wstring> get_steam_path();

// checks the windows uninstall registry for steam apps
[[nodiscard]] std::optional<std::wstring> get_app_install_from_uninstall(int id);

// searches all steam libraries for the game by parsing manifest files
[[nodiscard]] std::optional<std::wstring> get_app_install_by_manifests(const std::wstring& steam_path, int id);

// tries to get the current logged in steam user's steamid64
[[nodiscard]] std::optional<std::wstring> get_current_steamid64(const std::wstring& steam_path);

// reads libraryfolders.vdf to find all steam library locations
[[nodiscard]] std::vector<std::wstring> get_library_roots(const std::wstring& steam_path);

// parses steam manifest files (.acf) to get the install directory name
[[nodiscard]] std::optional<std::string> get_install_dir_from_manifest(const std::filesystem::path& manifest);