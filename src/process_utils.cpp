#include "process_utils.h"
#include <windows.h>
#include <tlhelp32.h>
#include <map>
#include <chrono>

[[nodiscard]] bool is_process_running(const std::wstring_view exe_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wstr(exe_name).c_str()) == 0) { 
                found = true; 
                break; 
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

[[nodiscard]] bool start_process(const fs::path& exe, const wstr& args, const fs::path& cwd) {
    wstr cmd = L"\"";
    cmd.append(exe.wstring());
    cmd.append(L"\" ");
    cmd.append(args);
    
    std::vector cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{ .cb = sizeof(si) };
    PROCESS_INFORMATION pi{};
    const wstr cwdW = cwd.wstring();
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, cwdW.c_str(), &si, &pi);
    if (ok) { 
        CloseHandle(pi.hThread); 
        CloseHandle(pi.hProcess); 
    }
    return ok == TRUE;
}

[[nodiscard]] bool ensure_steam_running(const wstr& steam_path, const int timeout_sec) {
    if (is_process_running(L"steam.exe")) return true;
    const fs::path steam_exe = fs::path(steam_path) / L"steam.exe";
    // launch steam in silent mode so it doesnt spam the user with windows
    (void)start_process(steam_exe, L"-silent", fs::path(steam_path));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    while (std::chrono::steady_clock::now() < deadline) {
        if (is_process_running(L"steam.exe")) return true;
        Sleep(500);
    }
    return false;
}

[[nodiscard]] std::vector<wchar_t> make_environment_with_overrides(const std::vector<std::pair<wstr, wstr>>& overrides) {
    std::map<wstr, wstr, std::less<>> env;
    // copy current environment
    if (LPWCH block = GetEnvironmentStringsW()) {
        for (const wchar_t* p = block; *p; ) {
            wstr entry = p;
            p += entry.size() + 1;
            if (const auto pos = entry.find(L'='); pos != wstr::npos && pos != 0) {
                wstr key = entry.substr(0, pos);
                const wstr val = entry.substr(pos + 1);
                env[key] = val;
            }
        }
        FreeEnvironmentStringsW(block);
    }
    // apply our overrides
    for (const auto& [k, v] : overrides) env[k] = v;
    // build the environment block (null terminated strings followed by a final null)
    std::vector<wchar_t> out;
    for (const auto& [k, v] : env) {
        wstr line;
        line.reserve(k.size() + 1 + v.size());
        line = k;
        line += L"=";
        line += v;
        out.insert(out.end(), line.begin(), line.end());
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}