#pragma once

#include "common.h"
#include <string_view>
#include <vector>

// checks if a process with the given exe name is running
[[nodiscard]] bool is_process_running(std::wstring_view exe_name);

// spawns a new process with the given args and working dir
[[nodiscard]] bool start_process(const fs::path& exe, const wstr& args, const fs::path& cwd);

// makes sure steam is running before we launch the game
[[nodiscard]] bool ensure_steam_running(const wstr& steam_path, int timeout_sec);

// builds a new environment block with our custom vars injected
[[nodiscard]] std::vector<wchar_t> make_environment_with_overrides(const std::vector<std::pair<wstr, wstr>>& overrides);