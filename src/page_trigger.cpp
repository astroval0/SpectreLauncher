#include "page_trigger.h"
#include <iostream>
#include <tlhelp32.h>
#include <winhttp.h>
#include <optional>
#include <string>

namespace {
    inline constexpr std::uintptr_t TARGET_RVA = 0x320B000;

    [[nodiscard]] std::string narrow_ascii(const wstr& w) {
        std::string s;
        s.reserve(w.size());
        for (wchar_t ch : w) {
            s.push_back(static_cast<char>(ch));
        }
        return s;
    }

    [[nodiscard]] std::optional<std::uintptr_t> get_main_module_base(DWORD pid) {
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
            pid
        );
        if (snap == INVALID_HANDLE_VALUE) {
            return std::nullopt;
        }

        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);

        std::optional<std::uintptr_t> base;
        if (Module32FirstW(snap, &me)) {
            base = reinterpret_cast<std::uintptr_t>(me.modBaseAddr);
        }

        CloseHandle(snap);
        return base;
    }

    [[nodiscard]] bool wait_for_target_rva_readable(HANDLE process, DWORD pid) {
        constexpr std::uintptr_t rva = TARGET_RVA;

        std::cout << "waiting for player to press start in-game..." << std::endl;
        Sleep(5000);

        std::optional<std::uintptr_t> base;

        for (;;) {
            DWORD code = 0;
            if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) {
                return false;
            }

            if (!base) {
                base = get_main_module_base(pid);
            }

            if (base) {
                const void* addr = reinterpret_cast<const void*>(*base + rva);
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQueryEx(process, addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
                    if (mbi.State == MEM_COMMIT &&
                        !(mbi.Protect & PAGE_GUARD) &&
                        !(mbi.Protect & PAGE_NOACCESS)) {

                        const DWORD prot = mbi.Protect & 0xffu;
                        if (prot == PAGE_READONLY ||
                            prot == PAGE_READWRITE ||
                            prot == PAGE_WRITECOPY ||
                            prot == PAGE_EXECUTE_READ ||
                            prot == PAGE_EXECUTE_READWRITE ||
                            prot == PAGE_EXECUTE_WRITECOPY) {
                            std::cout << "player pressed start" << std::endl;
                            return true;
                        }
                    }
                }
            }

            Sleep(1);
        }
    }

    [[nodiscard]] bool submit_provider_id(const wstr& steamId) {
        static constexpr wchar_t HOST[] = L"game.spectre.astro-dev.uk";
        static constexpr wchar_t PATH[] = L"/v1/submitproviderid";
        static constexpr INTERNET_PORT PORT = 80;
        static constexpr DWORD REQUEST_FLAGS = 0;

        HINTERNET hSession = WinHttpOpen(
            L"SpectreLauncher/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            nullptr,
            nullptr,
            0
        );
        if (!hSession) {
            return false;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, HOST, PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            PATH,
            nullptr,
            nullptr,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            REQUEST_FLAGS
        );
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        const std::string id   = narrow_ascii(steamId);
        const std::string body = std::string(R"({"providerId":")") + id + R"("})";

        BOOL ok = WinHttpSendRequest(
            hRequest,
            L"Content-Type: application/json\r\n",
            -1L,
            const_cast<void*>(static_cast<const void*>(body.data())),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0
        );
        if (!ok) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        ok = WinHttpReceiveResponse(hRequest, nullptr);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ok == TRUE;
    }
} // anon namespace

bool RunPageTrigger(HANDLE processHandle, DWORD pid, const wstr& steamId) {
    if (!processHandle || steamId.empty()) {
        return false;
    }

    const bool rva_ok = wait_for_target_rva_readable(processHandle, pid);
    if (!rva_ok) {
        return false;
    }

    return submit_provider_id(steamId);
}