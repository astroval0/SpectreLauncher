#include "registry_utils.h"

[[nodiscard]] op reg_read_sz(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    HKEY h{};
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &h) != ERROR_SUCCESS) return std::nullopt;
    DWORD type = 0, size = 0;
    op out;
    if (RegGetValueW(h, nullptr, value, RRF_RT_REG_SZ, &type, nullptr, &size) == ERROR_SUCCESS && size >= sizeof(wchar_t)) {
        wstr buf(size / sizeof(wchar_t), L'\0');
        if (RegGetValueW(h, nullptr, value, RRF_RT_REG_SZ, &type, buf.data(), &size) == ERROR_SUCCESS) {
            buf.resize(size / sizeof(wchar_t) ? size / sizeof(wchar_t) - 1 : 0);
            out = buf;
        }
    }
    RegCloseKey(h);
    return out;
}