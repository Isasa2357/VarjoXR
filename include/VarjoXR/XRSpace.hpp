#pragma once

#include <VarjoXR/BackendType.hpp>
#include <VarjoXR/Math.hpp>
#include <VarjoXR/Texture.hpp>
#include <VarjoXR/XRPlane.hpp>

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
class IRenderBackend;
class XRObject;

struct XRSpaceConfig {
    BackendType backend = BackendType::D3D11;
    int32_t swapChainTextureCount = 3;
    int64_t layerFlags = 0;
    // 0 means "use VarjoXR default". Runtime backends currently resolve it to
    // varjo_TextureFormat_R8G8B8A8_UNORM so that users do not accidentally pass
    // varjo_TextureFormat_INVALID to swapchain creation.
    int64_t colorFormat = 0;
    bool enableDebug = true;
};

class XRSpace {
public:
    explicit XRSpace(const XRSpaceConfig& config = {});
    ~XRSpace();

    XRSpace(const XRSpace&) = delete;
    XRSpace& operator=(const XRSpace&) = delete;

    XRPlane* createPlane(Vec2 size);

    std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes = 0);

#if defined(VARJOXR_ENABLE_D3D11)
    std::shared_ptr<XRTexture> createTextureFromD3D11Resource(
        ID3D11Texture2D* texture,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN);
    std::shared_ptr<XRTexture> createTextureFromD3D11Srv(
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height);
#endif

#if defined(VARJOXR_ENABLE_D3D12)
    std::shared_ptr<XRTexture> createTextureFromD3D12Resource(
        ID3D12Resource* resource,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN);
#endif

    void update();

    VarjoSession& session();
    const XRSpaceConfig& config() const noexcept { return m_config; }

private:
    XRSpaceConfig m_config{};
    std::unique_ptr<VarjoSession> m_session;
    std::unique_ptr<IRenderBackend> m_backend;
    std::vector<std::unique_ptr<XRObject>> m_objects;
};

} // namespace VarjoXR
