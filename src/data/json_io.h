// json_io.h - declares common JSON file I/O result types and helpers.
// Boundary: schema validation belongs to the caller after reading JSON.

#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace mmx::json_io {

enum class ReadError {
    None,
    Open,
    Parse,
    Schema,
};

struct ReadResult {
    bool ok = false;
    nlohmann::json value;
    ReadError error = ReadError::None;
    std::string message;
};

ReadResult readJsonFromFile(const std::filesystem::path& path);
ReadResult readJsonObjectFromFile(const std::filesystem::path& path);
bool writeAtomically(const std::filesystem::path& path, const nlohmann::json& value, int indent = 2);

} // namespace mmx::json_io
