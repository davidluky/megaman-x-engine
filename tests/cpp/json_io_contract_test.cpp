// json_io_contract_test.cpp - result contract for src/data/json_io.{h,cpp}.
//
// Proves: open failure yields ReadError::Open (!ok, non-empty message);
//   malformed JSON yields ReadError::Parse; non-object JSON (array) via
//   readJsonObjectFromFile yields ReadError::Schema while readJsonFromFile
//   accepts the same array; a valid object read via
//   readJsonObjectFromFile reports ok with the value preserved; an atomic
//   write/reload round-trip via writeAtomically + readJsonObjectFromFile
//   preserves nested object/array/number/bool values and returns true.
// Does NOT prove: any caller schema validation beyond the object check,
//   durability across crashes, or behavior with content packs, ROMs, or KB.
//
// Boundary: temp files under std::filesystem::temp_directory_path() only;
//   no window, assets, ROM, content pack, or KB access.

#include "data/json_io.h"

#include <nlohmann/json.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;
namespace jio = mmx::json_io;

void CheckOpenFailure(const fs::path& dir) {
    const jio::ReadResult result = jio::readJsonObjectFromFile(dir / "missing.json");
    assert(!result.ok);
    assert(result.error == jio::ReadError::Open);
    assert(!result.message.empty());
}

void CheckParseFailure(const fs::path& dir) {
    const fs::path path = dir / "malformed.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file.is_open());
        file << "{ not valid json,";
    }
    const jio::ReadResult result = jio::readJsonObjectFromFile(path);
    assert(!result.ok);
    assert(result.error == jio::ReadError::Parse);
    assert(!result.message.empty());
}

void CheckSchemaBoundary(const fs::path& dir) {
    const fs::path path = dir / "array.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file.is_open());
        file << "[1, 2, 3]";
    }
    const jio::ReadResult loose = jio::readJsonFromFile(path);
    assert(loose.ok);
    assert(loose.error == jio::ReadError::None);
    assert(loose.value.is_array());

    const jio::ReadResult strict = jio::readJsonObjectFromFile(path);
    assert(!strict.ok);
    assert(strict.error == jio::ReadError::Schema);
    assert(!strict.message.empty());
}

void CheckObjectRead(const fs::path& dir) {
    const fs::path path = dir / "object.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file.is_open());
        file << "{\"name\": \"zero\", \"hp\": 32}";
    }
    const jio::ReadResult result = jio::readJsonObjectFromFile(path);
    assert(result.ok);
    assert(result.error == jio::ReadError::None);
    assert(result.value.is_object());
    assert(result.value.at("name") == "zero");
    assert(result.value.at("hp") == 32);
}

void CheckAtomicRoundTrip(const fs::path& dir) {
    const fs::path path = dir / "roundtrip.json";
    const nlohmann::json expected = {
        {"nested", {{"count", 7}, {"enabled", true}}},
        {"items", {1, 2, 3}},
        {"ratio", 1.5},
        {"flag", false},
    };
    assert(jio::writeAtomically(path, expected));
    const jio::ReadResult result = jio::readJsonObjectFromFile(path);
    assert(result.ok);
    assert(result.error == jio::ReadError::None);
    assert(result.value == expected);
    assert(result.value.at("nested").at("count") == 7);
    assert(result.value.at("nested").at("enabled") == true);
    assert(result.value.at("items").size() == 3);
    assert(result.value.at("ratio") == 1.5);
    assert(result.value.at("flag") == false);
}

}  // namespace

int main() {
    const fs::path dir =
        fs::temp_directory_path() / "mmx-json-io-contract-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    assert(!ec);

    CheckOpenFailure(dir);
    CheckParseFailure(dir);
    CheckSchemaBoundary(dir);
    CheckObjectRead(dir);
    CheckAtomicRoundTrip(dir);

    fs::remove_all(dir, ec);
    return 0;
}
