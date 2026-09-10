// trace_writer.h - writes structured trace rows to diagnostic output streams.
// Boundary: serialization helper only; row producers own trace semantics.

#pragma once

#include <cstdio>
#include <cstdlib>

namespace mmx {

class TraceWriter {
public:
    TraceWriter() = default;
    ~TraceWriter() { reset(); }

    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;
    TraceWriter(TraceWriter&&) = delete;
    TraceWriter& operator=(TraceWriter&&) = delete;

    bool openFromEnv(const char* envVar, const char* headerText) {
        if (file_) return true;
        const char* path = std::getenv(envVar);
        if (!path || *path == '\0') return false;

        file_ = std::fopen(path, "w");
        if (!file_) return false;

        if (headerText && *headerText && std::fputs(headerText, file_) == EOF) {
            reset();
            return false;
        }
        return true;
    }

    void reset() {
        if (!file_) return;
        std::fclose(file_);
        file_ = nullptr;
    }

    FILE* file() const { return file_; }
    explicit operator bool() const { return file_ != nullptr; }

private:
    FILE* file_ = nullptr;
};

} // namespace mmx
