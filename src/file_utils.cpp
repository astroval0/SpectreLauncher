#include "file_utils.h"
#include <windows.h>
#include <urlmon.h>
#include <bcrypt.h>
#include <fstream>
#include <chrono>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "bcrypt.lib")

[[nodiscard]] wstr wtrim_trailing_slash(wstr s) {
    while (!s.empty() && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    return s;
}

[[nodiscard]] std::optional<std::vector<unsigned char>> sha256_bytes_of_stream(std::istream& is) {
    BCRYPT_ALG_HANDLE hAlg{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0) return std::nullopt;

    DWORD cbHashObject = 0, cbData = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&cbHashObject), sizeof(DWORD), &cbData, 0);
    if (status != 0) {
        (void)BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    std::vector<UCHAR> hashObject(cbHashObject);
    BCRYPT_HASH_HANDLE hHash{};
    status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), static_cast<ULONG>(hashObject.size()), nullptr, 0, 0);
    if (status != 0) {
        (void)BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    // hash the file in chunks so we dont blow up memory on big files
    std::vector<char> buf(1 << 16);
    while (is) {
        is.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        if (const std::streamsize got = is.gcount(); got > 0) {
            status = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buf.data()), static_cast<ULONG>(got), 0);
            if (status != 0) {
                (void)BCryptDestroyHash(hHash);
                (void)BCryptCloseAlgorithmProvider(hAlg, 0);
                return std::nullopt;
            }
        }
    }

    DWORD cbHash = 0;
    status = BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&cbHash), sizeof(DWORD), &cbData, 0);
    if (status != 0) {
        (void)BCryptDestroyHash(hHash);
        (void)BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    std::vector<unsigned char> hash(cbHash);
    status = BCryptFinishHash(hHash, hash.data(), cbHash, 0);
    if (status != 0) {
        (void)BCryptDestroyHash(hHash);
        (void)BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    (void)BCryptDestroyHash(hHash);
    (void)BCryptCloseAlgorithmProvider(hAlg, 0);
    return hash;
}

[[nodiscard]] std::optional<std::string> sha256_file(const fs::path& p) {
    if (std::error_code ec; !fs::exists(p, ec)) return std::nullopt;
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return std::nullopt;
    const auto bytes = sha256_bytes_of_stream(ifs);
    if (!bytes) return std::nullopt;
    static auto hex = "0123456789ABCDEF";
    std::string out; 
    out.resize(bytes->size() * 2);
    for (size_t i = 0; i < bytes->size(); ++i) {
        out[2 * i + 0] = hex[((*bytes)[i] >> 4) & 0xF];
        out[2 * i + 1] = hex[(*bytes)[i] & 0xF];
    }
    return out;
}

[[nodiscard]] bool download_to_file(const wchar_t* url, const fs::path& dst) {
    const fs::path parent = dst.parent_path();
    std::error_code ec;
    if (!parent.empty()) fs::create_directories(parent, ec);
    const HRESULT hr = URLDownloadToFileW(nullptr, url, dst.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
}

[[nodiscard]] bool clear_directory(const fs::path& dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec)) return true;
    if (ec) return false;

    bool success = true;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        std::error_code ec2;
        fs::remove_all(entry, ec2);
        if (ec2) success = false;
    }
    if (ec) return false;
    return success;
}

[[nodiscard]] wstr get_temp_file_guid() {
    wchar_t tmpDir[MAX_PATH];
    if (const DWORD n = GetTempPathW(MAX_PATH, tmpDir); n == 0 || n > MAX_PATH) return L".\\";
    GUID g{};
    if (const HRESULT hr = CoCreateGuid(&g); FAILED(hr)) {
        // fallback to timestamp if guid creation fails
        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        wchar_t name[64];
        swprintf_s(name, L"temp_%lld.tmp", static_cast<long long>(now));
        const fs::path p = fs::path(tmpDir) / name;
        return p.wstring();
    }
    wchar_t name[64];
    swprintf_s(name, L"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX.tmp",
               g.Data1, g.Data2, g.Data3,
               g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
               g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    const fs::path p = fs::path(tmpDir) / name;
    return p.wstring();
}