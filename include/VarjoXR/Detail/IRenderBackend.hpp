#pragma once

#include <VarjoXR/BackendType.hpp>
#include <VarjoXR/Texture.hpp>
#include <VarjoXR/XRObject.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace VarjoXR {

class VarjoSession;
struct XRSpaceConfig;

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual BackendType type() const noexcept = 0;
    virtual void initialize(VarjoSession& session, const XRSpaceConfig& config) = 0;
    virtual void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) = 0;
    virtual std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) = 0;
};

} // namespace VarjoXR
