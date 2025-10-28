#pragma once

#define UNICODE
#define NOMINMAX

#include <string>
#include <optional>
#include <filesystem>

namespace fs = std::filesystem;
using wstr = std::wstring;
using op = std::optional<wstr>;

// spectre's steam appid
inline constexpr int APP_ID = 2641470;
inline constexpr auto APP_ID_STR = L"2641470";
// this is the patched BE dll that bypasses the game's init checks and applies the hook.
inline constexpr auto RELEASE_URL = L"https://github.com/astroval0/SpectrePatcher/releases/latest/download/BEClient_x64.dll";
// pragmabackend addr
inline constexpr auto BACKEND_ADDRESS = L"http://game.spectre.astro-dev.uk:8081";