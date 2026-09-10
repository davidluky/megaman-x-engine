// PCM regions retain a one-time intro and wrap inside each submitted block.
// R359: knowledge_base/mmx1/audio/flame_mammoth_loop.json.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mmx {

class PcmLoop {
public:
    bool load(const std::int16_t* samples, std::size_t frames,
              unsigned channels, std::size_t begin, std::size_t end) {
        clear();
        if (!samples || channels == 0 || channels > 2 || begin >= end || end > frames)
            return false;
        if (end > samples_.max_size() / channels) return false;
        samples_.assign(samples, samples + end * channels);
        channels_ = channels;
        begin_ = begin;
        end_ = end;
        return true;
    }

    void clear() {
        samples_.clear();
        channels_ = 0;
        begin_ = end_ = cursor_ = 0;
    }

    bool valid() const { return channels_ != 0; }
    void rewind() { cursor_ = 0; }

    bool fill(std::int16_t* output, std::size_t frames) {
        if (!valid() || !output) return false;
        while (frames != 0) {
            if (cursor_ == end_) cursor_ = begin_;
            const auto count = std::min(frames, end_ - cursor_);
            std::memcpy(output, samples_.data() + cursor_ * channels_,
                        count * channels_ * sizeof(std::int16_t));
            output += count * channels_;
            cursor_ += count;
            frames -= count;
        }
        return true;
    }

private:
    std::vector<std::int16_t> samples_;
    std::size_t begin_ = 0, end_ = 0, cursor_ = 0;
    unsigned channels_ = 0;
};

} // namespace mmx
