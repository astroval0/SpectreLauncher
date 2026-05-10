#pragma once
#include "common.h"
#include <string_view>

// checks if a process with the given exe name is running
[[nodiscard]] bool is_process_running(std::wstring_view exe_name);