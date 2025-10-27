#pragma once

#include "common.h"
#include <vector>
#include <istream>

// just trims trail slashes from paths cause windows is shit
[[nodiscard]] wstr wtrim_trailing_slash(wstr s);

// computes sha256 hash of a stream using bcrypt api
[[nodiscard]] std::optional<std::vector<unsigned char>> sha256_bytes_of_stream(std::istream& is);

// wrapper around sha256_bytes_of_stream that returns a hex string
[[nodiscard]] std::optional<std::string> sha256_file(const fs::path& p);

// downloads a file from url to dst using urlmon (old school windows api lol)
[[nodiscard]] bool download_to_file(const wchar_t* url, const fs::path& dst);

// del everything in a directory but keeps the directory itself
[[nodiscard]] bool clear_directory(const fs::path& dir);

// generates a temp file path using a guid
[[nodiscard]] wstr get_temp_file_guid();