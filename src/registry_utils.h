#pragma once

#include "common.h"
#include <windows.h>

// reads a string value from registry, returns nullopt if it doesnt exist or errors out
[[nodiscard]] op reg_read_sz(HKEY root, const wchar_t* subkey, const wchar_t* value);