#pragma once

#include <cstdint>

#include <VarjoXR/Foundation/BackendType.hpp>

namespace VarjoXR {

enum class TextureOwnership {
    Owned,
    External,
};

class XRTexture {
public:
    XRTexture(BackendType backend, uint32_t width, uint32_t height, TextureOwnership ownership) noexcept
        : backend_(backend), width_(width), height_(height), ownership_(ownership) {}
    virtual ~XRTexture() = default;

    BackendType backend() const noexcept { return backend_; }
    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }
    TextureOwnership ownership() const noexcept { return ownership_; }

private:
    BackendType backend_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    TextureOwnership ownership_ = TextureOwnership::External;
};

} // namespace VarjoXR
