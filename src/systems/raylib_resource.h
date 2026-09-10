// raylib_resource.h - wraps raylib resources with RAII lifetime ownership.
// Owns: texture, render texture, image, music, and sound unload semantics.

#pragma once

#include "raylib.h"
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace mmx {

class TextureResource {
public:
    TextureResource() = default;
    ~TextureResource() { reset(); }

    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;

    TextureResource(TextureResource&& other) noexcept
        : texture_(std::exchange(other.texture_, {})) {}

    TextureResource& operator=(TextureResource&& other) noexcept {
        if (this != &other) {
            reset();
            texture_ = std::exchange(other.texture_, {});
        }
        return *this;
    }

    bool load(const char* path) {
        reset();
        texture_ = LoadTexture(path);
        return valid();
    }

    bool load(const std::string& path) {
        return load(path.c_str());
    }

    bool loadFromImage(const Image& image) {
        reset();
        texture_ = LoadTextureFromImage(image);
        return valid();
    }

    void reset() {
        if (texture_.id != 0) {
            UnloadTexture(texture_);
        }
        texture_ = {};
    }

    bool valid() const { return texture_.id != 0; }
    unsigned int id() const { return texture_.id; }
    int width() const { return texture_.width; }
    int height() const { return texture_.height; }
    const Texture2D& get() const { return texture_; }

    void setFilter(int filter) const {
        if (valid()) {
            SetTextureFilter(texture_, filter);
        }
    }

private:
    Texture2D texture_ = {};
};

class RenderTextureResource {
public:
    RenderTextureResource() = default;
    ~RenderTextureResource() { reset(); }

    RenderTextureResource(const RenderTextureResource&) = delete;
    RenderTextureResource& operator=(const RenderTextureResource&) = delete;

    RenderTextureResource(RenderTextureResource&& other) noexcept
        : target_(std::exchange(other.target_, {})) {}

    RenderTextureResource& operator=(RenderTextureResource&& other) noexcept {
        if (this != &other) {
            reset();
            target_ = std::exchange(other.target_, {});
        }
        return *this;
    }

    bool load(int width, int height) {
        reset();
        target_ = LoadRenderTexture(width, height);
        return valid();
    }

    void reset() {
        if (valid()) {
            UnloadRenderTexture(target_);
        }
        target_ = {};
    }

    bool valid() const { return target_.id != 0 && target_.texture.id != 0; }
    const RenderTexture2D& get() const { return target_; }
    const Texture2D& texture() const { return target_.texture; }

    void setTextureFilter(int filter) const {
        if (valid()) {
            SetTextureFilter(target_.texture, filter);
        }
    }

private:
    RenderTexture2D target_ = {};
};

class ImageResource {
public:
    ImageResource() = default;
    ~ImageResource() { reset(); }

    ImageResource(const ImageResource&) = delete;
    ImageResource& operator=(const ImageResource&) = delete;

    ImageResource(ImageResource&& other) noexcept
        : image_(other.image_) {
        other.image_ = {};
    }

    ImageResource& operator=(ImageResource&& other) noexcept {
        if (this != &other) {
            reset();
            image_ = other.image_;
            other.image_ = {};
        }
        return *this;
    }

    bool load(const char* path) {
        reset();
        image_ = LoadImage(path);
        return valid();
    }

    bool load(const std::string& path) {
        return load(path.c_str());
    }

    bool loadFromTexture(Texture2D texture) {
        reset();
        image_ = LoadImageFromTexture(texture);
        return valid();
    }

    bool loadFromPixels(const Color* pixels, int width, int height) {
        reset();
        if (!pixels || width <= 0 || height <= 0) return false;
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixelCount > std::numeric_limits<unsigned int>::max() / sizeof(Color)) {
            return false;
        }
        image_.data = MemAlloc(static_cast<unsigned int>(pixelCount * sizeof(Color)));
        if (!image_.data) {
            image_ = {};
            return false;
        }
        std::memcpy(image_.data, pixels, pixelCount * sizeof(Color));
        image_.width = width;
        image_.height = height;
        image_.mipmaps = 1;
        image_.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return true;
    }

    void reset() {
        if (image_.data) {
            UnloadImage(image_);
            image_ = {};
        }
    }

    bool valid() const { return image_.data != nullptr; }
    int width() const { return image_.width; }
    int height() const { return image_.height; }
    Color* pixels() { return valid() ? static_cast<Color*>(image_.data) : nullptr; }

    bool uploadTo(TextureResource& texture) const {
        return valid() && texture.loadFromImage(image_);
    }

    void format(int pixelFormat) {
        if (valid()) {
            ImageFormat(&image_, pixelFormat);
        }
    }

    void flipVertical() {
        if (valid()) {
            ImageFlipVertical(&image_);
        }
    }

    bool exportTo(const char* path) const {
        return valid() && ExportImage(image_, path);
    }

private:
    Image image_ = {};
};

class MusicResource {
public:
    MusicResource() = default;
    ~MusicResource() { reset(); }

    MusicResource(const MusicResource&) = delete;
    MusicResource& operator=(const MusicResource&) = delete;

    MusicResource(MusicResource&& other) noexcept
        : music_(std::exchange(other.music_, {})) {}

    MusicResource& operator=(MusicResource&& other) noexcept {
        if (this != &other) {
            reset();
            music_ = std::exchange(other.music_, {});
        }
        return *this;
    }

    bool load(const char* path) {
        reset();
        music_ = LoadMusicStream(path);
        return valid();
    }

    bool load(const std::string& path) {
        return load(path.c_str());
    }

    void reset() {
        if (valid()) {
            UnloadMusicStream(music_);
        }
        music_ = {};
    }

    bool valid() const { return music_.stream.sampleRate != 0; }
    const Music& get() const { return music_; }

    void setLooping(bool looping) {
        music_.looping = looping;
    }

private:
    Music music_ = {};
};

class SoundResource {
public:
    SoundResource() = default;
    ~SoundResource() { reset(); }

    SoundResource(const SoundResource&) = delete;
    SoundResource& operator=(const SoundResource&) = delete;

    SoundResource(SoundResource&& other) noexcept
        : sound_(std::exchange(other.sound_, {})) {}

    SoundResource& operator=(SoundResource&& other) noexcept {
        if (this != &other) {
            reset();
            sound_ = std::exchange(other.sound_, {});
        }
        return *this;
    }

    bool load(const char* path) {
        reset();
        sound_ = LoadSound(path);
        return valid();
    }

    bool load(const std::string& path) {
        return load(path.c_str());
    }

    void reset() {
        if (valid()) {
            UnloadSound(sound_);
        }
        sound_ = {};
    }

    bool valid() const { return sound_.frameCount != 0; }
    const Sound& get() const { return sound_; }

    void setVolume(float volume) const {
        if (valid()) {
            SetSoundVolume(sound_, volume);
        }
    }

    void play() const {
        if (valid()) {
            PlaySound(sound_);
        }
    }

    void stop() const {
        if (valid()) {
            StopSound(sound_);
        }
    }

    bool isPlaying() const {
        return valid() && IsSoundPlaying(sound_);
    }

private:
    Sound sound_ = {};
};

} // namespace mmx
