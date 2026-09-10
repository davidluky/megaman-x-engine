// logging.cpp - implements gameplay-scoped logging helpers.
// Boundary: diagnostics only; logging must not gate gameplay behavior.

#include "gameplay/logging.h"
#include "raylib.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace mmx::logging {

#ifdef NDEBUG
namespace {

constexpr int MaxRotatedLogs = 3;
std::ofstream gTraceLog;

const char* levelName(int logLevel) {
    switch (logLevel) {
    case LOG_TRACE: return "TRACE";
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO: return "INFO";
    case LOG_WARNING: return "WARN";
    case LOG_ERROR: return "ERROR";
    case LOG_FATAL: return "FATAL";
    default: return "LOG";
    }
}

std::string timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
    if (const std::tm* current = std::localtime(&now)) {
        localTime = *current;
    }
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime) == 0) {
        return "0000-00-00 00:00:00";
    }
    return buffer;
}

std::filesystem::path rotatedLogPath(const std::filesystem::path& dir, int index) {
    return dir / ("megaman-x." + std::to_string(index) + ".log");
}

void rotateLogs(const std::filesystem::path& dir) {
    std::error_code ec;
    for (int i = MaxRotatedLogs; i >= 1; --i) {
        const std::filesystem::path target = rotatedLogPath(dir, i);
        std::filesystem::remove(target, ec);
        ec.clear();

        const std::filesystem::path source =
            (i == 1) ? dir / "megaman-x.log" : rotatedLogPath(dir, i - 1);
        if (std::filesystem::exists(source, ec)) {
            ec.clear();
            std::filesystem::rename(source, target, ec);
            ec.clear();
        }
    }
}

void traceLogToFile(int logLevel, const char* text, va_list args) {
    if (!gTraceLog.is_open()) return;

    char message[2048];
    std::vsnprintf(message, sizeof(message), text, args);
    gTraceLog << timestamp() << " [" << levelName(logLevel) << "] " << message << '\n';
    gTraceLog.flush();
}

} // namespace
#endif

void installReleaseTraceLog(bool hiddenAutomation) {
#ifdef NDEBUG
    // PX6A: this sink REPLACES raylib's default stdout logging, so in a
    // release build it swallows the automation completion markers
    // ("Smoke: ... done", the EN4 frame count) that CTest and subprocess
    // capture match on. Hidden automation therefore skips it entirely and
    // keeps the default stdout sink. Interactive release play is unaffected:
    // it still gets the rotating logs below. Skipping BEFORE create/rotate
    // also means hidden runs never touch logs/ at all.
    if (hiddenAutomation) return;

    const std::filesystem::path logDir = "logs";
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (ec) return;

    rotateLogs(logDir);
    gTraceLog.open(logDir / "megaman-x.log", std::ios::out | std::ios::trunc);
    if (!gTraceLog.is_open()) return;

    gTraceLog << timestamp() << " [INFO] Mega Man X log started\n";
    gTraceLog.flush();
    SetTraceLogCallback(traceLogToFile);
#else
    (void)hiddenAutomation;
#endif
}

} // namespace mmx::logging
