#pragma once

#include <VarjoXR/BackendType.hpp>

#include <cstdint>
#include <memory>

namespace VarjoXR {

class XRTexture {
public:
    XRTexture(BackendType backend, uint32_t width, uint32_t height)
        : m_backend(backend), m_width(width), m_height(height) {}
    virtual ~XRTexture() = default;

    BackendType backend() const noexcept { return m_backend; }
    uint32_t width() const noexcept { return m_width; }
    uint32_t height() const noexcept { return m_height; }

private:
    BackendType m_backend;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

using XRTexturePtr = std::shared_ptr<XRTexture>;

} // namespace VarjoXR
