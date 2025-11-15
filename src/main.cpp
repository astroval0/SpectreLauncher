#include "common.h"
#include "steam_finder.h"
#include "process_utils.h"
#include "file_utils.h"
#include "page_trigger.h"
#include <windows.h>
#include <cstdio>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")

int wmain() {
    // init com for urlmon
    if (HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); FAILED(hr)) {
        std::fprintf(stderr, "Failed to initialize COM: 0x%08lX\n", hr);
        return 1;
    }

    // find steam installation
    auto steamOpt = get_steam_path();
    if (!steamOpt) {
        std::puts("Steam not installed.");
        CoUninitialize();
        return 1;
    }
    const wstr& steam = *steamOpt;

    // find game installation (try the uninstall registry first then manifests)
    auto gameRoot = get_app_install_from_uninstall(APP_ID);
    if (!gameRoot) gameRoot = get_app_install_by_manifests(steam, APP_ID);
    if (!gameRoot) {
        std::puts("Game not installed.");
        CoUninitialize();
        return 2;
    }

    // setup paths to game exe and BE directory
    fs::path binDir = fs::path(*gameRoot) / L"Spectre" / L"Binaries" / L"Win64";
    fs::path beDir = binDir / L"BattlEye";
    fs::path beClient = beDir / L"BEClient_x64.dll";
    fs::path clientExe = binDir / L"SpectreClient-Win64-Shipping.exe";

    if (!fs::exists(clientExe)) {
        std::puts("Client executable not found.");
        CoUninitialize();
        return 3;
    }
    if (std::error_code ec; !fs::exists(beDir) && !fs::create_directories(beDir, ec)) {
        std::fprintf(stderr, "Failed to create BattlEye directory: %s\n", ec.message().c_str());
        CoUninitialize();
        return 4;
    }

    // check if we need to download / update the patched BE dll
    std::optional<std::string> installedHash;
    if (fs::exists(beClient)) installedHash = sha256_file(beClient);

    fs::path tempFile = get_temp_file_guid();
    std::optional<std::string> downloadHash;
    if (!download_to_file(RELEASE_URL, tempFile)) {
        std::fprintf(stderr, "Download failed.\n");
        if (fs::exists(tempFile)) {
            std::error_code ec2;
            fs::remove(tempFile, ec2);
        }
        CoUninitialize();
        return 5;
    }
    downloadHash = sha256_file(tempFile);
    if (!downloadHash) {
        std::fprintf(stderr, "Hash failed.\n");
        if (fs::exists(tempFile)) {
            std::error_code ec2;
            fs::remove(tempFile, ec2);
        }
        CoUninitialize();
        return 5;
    }

    // only replace BE dll if hash differs
    if (installedHash && *installedHash == *downloadHash) {
        std::error_code ec2;
        fs::remove(tempFile, ec2);
    } else {
        // del the old BE dir and install the new dll
        if (!clear_directory(beDir)) {
            std::fprintf(stderr, "Failed to clear BattlEye directory.\n");
            if (fs::exists(tempFile)) {
                std::error_code ec2;
                fs::remove(tempFile, ec2);
            }
            CoUninitialize();
            return 6;
        }
        std::error_code ec3;
        fs::create_directories(beDir, ec3);
        std::error_code ec4;
        fs::rename(tempFile, beClient, ec4);
        if (ec4) {
            // rename failed so fallback to copy + delete
            std::error_code ec5;
            fs::copy_file(tempFile, beClient, fs::copy_options::overwrite_existing, ec5);
            std::error_code ec6;
            fs::remove(tempFile, ec6);
            if (ec5) {
                std::fprintf(stderr, "Failed to install BEClient: %s\n", ec4.message().c_str());
                if (fs::exists(tempFile)) {
                    std::error_code ec7;
                    fs::remove(tempFile, ec7);
                }
                CoUninitialize();
                return 6;
            }
        }
    }

    // ensure steam is running (needed for auth and overlay bs)
    if (!ensure_steam_running(steam, 30)) {
        std::puts("Steam failed to start.");
        CoUninitialize();
        return 7;
    }

    // get current steam user's id
    auto steamIdOpt = get_current_steamid64(steam);
    wstr steamId = steamIdOpt.value_or(L"0");

    // setup env vars for steam overlay and our backend
    std::vector<std::pair<wstr, wstr>> overrides = {
        { L"STEAMID",            steamId },
        { L"SteamGameId",        APP_ID_STR },
        { L"SteamAppId",         APP_ID_STR },
        { L"SteamOverlayGameId", APP_ID_STR },
    };
    auto envBlock = make_environment_with_overrides(overrides);

    // point the game at our pragmabackend
    wstr args = L"-PragmaEnvironment=live -PragmaBackendAddress=";
    args.append(BACKEND_ADDRESS);
    wstr cmd = L"\"";

    cmd.append(clientExe.wstring());
    cmd.append(L"\" ");
    cmd.append(args);

    std::vector cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');
    wstr cwd = clientExe.parent_path().wstring();

    // launch the game with our envs
    STARTUPINFOW si{ .cb = sizeof(si) };
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(),
        cwd.c_str(),
        &si, &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        std::fprintf(stderr, "Failed to launch Spectre client: WinErr %lu\n", err);
        CoUninitialize();
        return 8;
    }

    RunPageTrigger(pi.hProcess, pi.dwProcessId, steamId);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CoUninitialize();
    return 0;
}