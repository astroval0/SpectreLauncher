#include "steam_finder.h"
#include "registry_utils.h"
#include "file_utils.h"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <regex>
#include <map>

#pragma comment(lib, "shlwapi.lib")

[[nodiscard]] op get_steam_path() {
    struct Key { HKEY root; const wchar_t* sub; };
    const Key keys[] = {
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam" },
        { HKEY_CURRENT_USER,  L"Software\\Valve\\Steam" },
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam" },
    };
    for (auto&[root, sub] : keys) {
        op path;
        // try different value names bc steam stores it under different keys
        if (auto v = reg_read_sz(root, sub, L"InstallPath"))      path = *v;
        else if (auto v2 = reg_read_sz(root, sub, L"SteamPath"))  path = *v2;
        else if (auto v3 = reg_read_sz(root, sub, L"InstallLocation")) path = *v3;
        else if (auto v4 = reg_read_sz(root, sub, L"DisplayIcon")) {
            // if we got the icon path we can extract the parent dir
            wstr exe = *v4;
            if (auto pos = exe.find(L','); pos != wstr::npos) exe = exe.substr(0, pos);
            wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
            _wsplitpath_s(exe.c_str(), drive, dir, fname, ext);
            wstr parent(drive);
            parent.append(dir);
            path = parent;
        }
        if (path) {
            std::error_code ec;
            auto norm = fs::weakly_canonical(fs::path(*path), ec);
            wstr cand = wtrim_trailing_slash(ec ? *path : norm.c_str());
            // validate we actually found steam by checking if steam.exe exists
            if (fs::path steam_exe = fs::path(cand) / L"steam.exe"; fs::exists(steam_exe)) return cand;
        }
    }
    return std::nullopt;
}

[[nodiscard]] op get_app_install_from_uninstall(const int id) {
    wstr key1 = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App " + std::to_wstring(id);
    wstr key2 = L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App " + std::to_wstring(id);
    for (const auto* pair : { &key1, &key2 }) {
        if (auto dir = reg_read_sz(HKEY_LOCAL_MACHINE, pair->c_str(), L"InstallLocation")) {
            fs::path p = wtrim_trailing_slash(*dir);
            if (fs::exists(p)) return p.c_str();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> get_install_dir_from_manifest(const fs::path& manifest) {
    if (std::error_code ec; !fs::exists(manifest, ec)) return std::nullopt;
    std::ifstream ifs(manifest, std::ios::binary);
    if (!ifs) return std::nullopt;
    std::string txt((std::istreambuf_iterator(ifs)), std::istreambuf_iterator<char>());
    // regex to find the installdir field
    std::regex re(R"==("installdir"\s*"([^"]+)")==", std::regex::icase);
    if (std::smatch m; std::regex_search(txt, m, re)) return m[1].str();
    return std::nullopt;
}

[[nodiscard]] std::vector<wstr> get_library_roots(const wstr& steam_path) {
    std::vector<wstr> roots;
    roots.push_back(wtrim_trailing_slash(steam_path));
    fs::path vdf = fs::path(steam_path) / L"steamapps" / L"libraryfolders.vdf";
    if (std::error_code ec; fs::exists(vdf, ec)) {
        if (std::ifstream ifs(vdf, std::ios::binary); ifs) {
            std::string txt((std::istreambuf_iterator(ifs)), std::istreambuf_iterator<char>());
            std::regex re(R"==("path"\s*"([^"]+)")==", std::regex::icase);
            for (auto it = std::sregex_iterator(txt.begin(), txt.end(), re); it != std::sregex_iterator(); ++it) {
                std::string raw = (*it)[1].str();
                // valve escapes backslashes as \\\\ in the vdf so we gotta unescape them
                std::string unesc; 
                unesc.reserve(raw.size());
                for (size_t i = 0; i < raw.size(); ++i) {
                    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') { 
                        unesc.push_back('\\'); 
                        ++i; 
                    } else {
                        unesc.push_back(raw[i]);
                    }
                }
                wstr w(unesc.begin(), unesc.end());
                if (auto t = wtrim_trailing_slash(w); !t.empty()) roots.push_back(t);
            }
        }
    }
    // dedupe the roots just in case
    std::vector<wstr> out;
    std::map<wstr, bool, std::less<>> seen;
    for (auto& r : roots) if (!seen.contains(r)) { seen[r] = true; out.push_back(r); }
    return out;
}

[[nodiscard]] op get_app_install_by_manifests(const wstr& steam_path, const int id) {
    for (const auto& root : get_library_roots(steam_path)) {
        fs::path steamapps = fs::path(root) / L"steamapps";
        fs::path manifest = steamapps / (L"appmanifest_" + std::to_wstring(id) + L".acf");
        if (auto dirName = get_install_dir_from_manifest(manifest)) {
            wstr wdir(dirName->begin(), dirName->end());
            fs::path candidate = steamapps / L"common" / wdir;
            if (std::error_code ec; fs::exists(candidate, ec)) return candidate.c_str();
        }
    }
    return std::nullopt;
}

[[nodiscard]] op get_current_steamid64(const wstr& steam_path) {
    // try registry first cause its fastest
    if (HKEY h; RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &h) == ERROR_SUCCESS) {
        DWORD val = 0, type = 0, size = sizeof(val);
        if (RegGetValueW(h, nullptr, L"ActiveUser", RRF_RT_REG_DWORD, &type, &val, &size) == ERROR_SUCCESS) {
            if (val != 0) {
                // convert steam3 id to steamid64 by adding the base offset
                constexpr unsigned long long base = 76561197960265728ULL;
                unsigned long long id64 = base + static_cast<unsigned long long>(val);
                RegCloseKey(h);
                return std::to_wstring(id64);
            }
        }
        RegCloseKey(h);
    }
    // fallback to parsing loginusers.vdf if registry fails
    fs::path vdf = fs::path(steam_path) / L"config" / L"loginusers.vdf";
    if (std::error_code ec; fs::exists(vdf, ec)) {
        if (std::ifstream ifs(vdf, std::ios::binary); ifs) {
            std::string txt((std::istreambuf_iterator(ifs)), std::istreambuf_iterator<char>());
            // look for user blocks and check if they have mostrecent flag set
            std::regex blockRe(R"==("(\d{17})"\s*\{([\s\S]*?)\})==", std::regex::icase);
            std::smatch m;
            std::string firstId;
            for (auto it = std::sregex_iterator(txt.begin(), txt.end(), blockRe); it != std::sregex_iterator(); ++it) {
                std::string id = (*it)[1].str();
                if (firstId.empty()) firstId = id;
                if (std::string block = (*it)[2].str(); std::regex_search(block, std::regex(R"==("mostrecent"\s*"\s*1")==", std::regex::icase))) {
                    wstr w(id.begin(), id.end());
                    return w;
                }
            }
            // if no mostrecent user just return the first one we found
            if (!firstId.empty()) {
                wstr w(firstId.begin(), firstId.end());
                return w;
            }
        }
    }
    return std::nullopt;
}