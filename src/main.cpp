#include "common.h"
#include "steam_finder.h"
#include "process_utils.h"
#include "file_utils.h"
#include <windows.h>
#include <cstdio>

#pragma comment(lib, "advapi32.lib")

int wmain() {
    // init com for urlmon
    if (HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); FAILED(hr)) {
        std::fprintf(stderr, "Failed to initialize COM: 0x%08lX\n", hr);
        return 1;
    }

    std::error_code ec;

    // find steam installation
    auto steamOpt = get_steam_path();
    if (!steamOpt) {
        std::puts("Steam not installed.");
        CoUninitialize();
        return 1;
    }
    const std::wstring &steam = *steamOpt;

    // find game installation (try the uninstall registry first then manifests)
    auto gameRoot = get_app_install_from_uninstall(APP_ID);
    if (!gameRoot) gameRoot = get_app_install_by_manifests(steam, APP_ID);
    if (!gameRoot) {
        std::puts("Game not installed.");
        CoUninitialize();
        return 2;
    }

    // setup paths to game exe and BE directory
    std::filesystem::path binDir = std::filesystem::path(*gameRoot) / L"Spectre" / L"Binaries" / L"Win64";
    std::filesystem::path beDir = binDir / L"BattlEye";
    std::filesystem::path beClient = beDir / L"BEClient_x64.dll";
    std::filesystem::path clientExe = binDir / L"SpectreClient-Win64-Shipping.exe";

    if (!std::filesystem::exists(clientExe, ec)) {
        std::puts("Client executable not found.");
        CoUninitialize();
        return 3;
    }
    if (!std::filesystem::exists(beDir, ec) && !std::filesystem::create_directories(beDir, ec)) {
        std::fprintf(stderr, "Failed to create BattlEye directory: %s\n", ec.message().c_str());
        CoUninitialize();
        return 4;
    }

    // check if we need to download / update the patched BE dll
    std::optional<std::string> installedHash;
    if (std::filesystem::exists(beClient, ec)) installedHash = sha256_file(beClient);

    std::filesystem::path tempFile = get_temp_file_guid();
    if (!download_to_file(RELEASE_URL, tempFile)) {
        std::fprintf(stderr, "Download failed.\n");
        std::filesystem::remove(tempFile, ec);
        CoUninitialize();
        return 5;
    }
    auto downloadHash = sha256_file(tempFile);
    if (!downloadHash) {
        std::fprintf(stderr, "Hash failed.\n");
        std::filesystem::remove(tempFile, ec);
        CoUninitialize();
        return 5;
    }

    // only replace BE dll if hash differs
    if (installedHash && *installedHash == *downloadHash) {
        std::filesystem::remove(tempFile, ec);
    } else {
        // del the old BE dir and install the new dll
        if (!clear_directory(beDir)) {
            std::fprintf(stderr, "Failed to clear BattlEye directory.\n");
            std::filesystem::remove(tempFile, ec);
            CoUninitialize();
            return 6;
        }
        std::filesystem::rename(tempFile, beClient, ec);
        if (ec) {
            // rename failed so fallback to copy + delete
            std::error_code copyEc;
            std::filesystem::copy_file(tempFile, beClient, std::filesystem::copy_options::overwrite_existing, copyEc);
            std::filesystem::remove(tempFile, ec);
            if (copyEc) {
                std::fprintf(stderr, "Failed to install BEClient: %s\n", copyEc.message().c_str());
                CoUninitialize();
                return 6;
            }
        }
    }

    // require steam to be running
    if (!is_process_running(L"steam.exe")) {
        std::puts("Steam is not running.");
        CoUninitialize();
        return 7;
    }

    // get current steam user's id
    auto steamIdOpt = get_current_steamid64(steam);
    std::wstring steamId = steamIdOpt.value_or(L"0");

    // setup env vars for steam overlay and our backend
    SetEnvironmentVariableW(L"STEAMID",     steamId.c_str());
    SetEnvironmentVariableW(L"SteamGameId", APP_ID_STR);
    SetEnvironmentVariableW(L"SteamAppId",  APP_ID_STR);
    SetEnvironmentVariableW(L"SteamOverlayGameId", APP_ID_STR);

    // point the game at our pragmabackend
    std::wstring cmd = L"\"";
    cmd.append(clientExe.wstring());
    cmd.append(L"\" -PragmaEnvironment=live -PragmaBackendAddress=");
    cmd.append(BACKEND_ADDRESS);

    std::wstring cwd = clientExe.parent_path().wstring();

    // launch the game with our envs
    STARTUPINFOW si{ .cb = sizeof(si) };
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr, nullptr, FALSE,
        0,
        nullptr,
        cwd.c_str(),
        &si, &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        std::fprintf(stderr, "Failed to launch Spectre client: WinErr %lu\n", err);
        CoUninitialize();
        return 8;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CoUninitialize();
    return 0;
}