#pragma once

#include <VarjoXR/BackendType.hpp>
#include <VarjoXR/Texture.hpp>
#include <VarjoXR/XRObject.hpp>

#include <cstdint>
#include <memory>
#include <vector>

#if defined(VARJOXR_ENABLE_D3D11)
#include <d3d11.h>
#endif
#if defined(VARJOXR_ENABLE_D3D12)
#include <d3d12.h>
#endif

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

#if defined(VARJOXR_ENABLE_D3D11)
    virtual std::shared_ptr<XRTexture> createTextureFromD3D11Resource(
        ID3D11Texture2D* texture,
        DXGI_FORMAT srvFormat) = 0;
    virtual std::shared_ptr<XRTexture> createTextureFromD3D11Srv(
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height) = 0;
#endif

#if defined(VARJOXR_ENABLE_D3D12)
    virtual std::shared_ptr<XRTexture> createTextureFromD3D12Resource(
        ID3D12Resource* resource,
        DXGI_FORMAT srvFormat) = 0;
#endif
};

} // namespace VarjoXR
