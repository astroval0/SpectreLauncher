#include "process_utils.h"
#include <windows.h>
#include <tlhelp32.h>

[[nodiscard]] bool is_process_running(const std::wstring_view exe_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    const std::wstring needle(exe_name);

    PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, needle.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}