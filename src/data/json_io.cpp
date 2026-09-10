// json_io.cpp - implements shared JSON read and atomic write helpers.
// Boundary: reports open/parse/schema failures without owning file schemas.

#include "data/json_io.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <fstream>
#include <system_error>

namespace mmx::json_io {

namespace {

std::string pathText(const std::filesystem::path& path) {
    return path.generic_string();
}

} // namespace

ReadResult readJsonFromFile(const std::filesystem::path& path) {
    ReadResult result;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.error = ReadError::Open;
        result.message = "failed to open JSON file " + pathText(path);
        return result;
    }

    try {
        file >> result.value;
    } catch (const nlohmann::json::exception& e) {
        result.error = ReadError::Parse;
        result.message = "failed to parse JSON file " + pathText(path) + ": " + e.what();
        return result;
    }

    result.ok = true;
    return result;
}

ReadResult readJsonObjectFromFile(const std::filesystem::path& path) {
    ReadResult result = readJsonFromFile(path);
    if (!result.ok) return result;
    if (!result.value.is_object()) {
        result.ok = false;
        result.error = ReadError::Schema;
        result.message = "expected JSON object in " + pathText(path);
    }
    return result;
}

bool writeAtomically(const std::filesystem::path& path, const nlohmann::json& value, int indent) {
    namespace fs = std::filesystem;

    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    const fs::path tempPath = path.string() + ".tmp";
    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        file << value.dump(indent);
        file.flush();
        if (!file.good()) {
            std::error_code ignored;
            fs::remove(tempPath, ignored);
            return false;
        }
    }

#ifdef _WIN32
    if (!MoveFileExA(tempPath.string().c_str(), path.string().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        fs::remove(tempPath, ignored);
        return false;
    }
#else
    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        return false;
    }
#endif

    return true;
}

} // namespace mmx::json_io
