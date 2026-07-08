#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <VarjoXR/Backends/IRenderBackend.hpp>
#include <VarjoXR/Core/XRTexture.hpp>

struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

typedef enum DXGI_FORMAT DXGI_FORMAT;

namespace VarjoXR::Backends::D3D11 {

struct D3D11BackendDesc {
    int32_t swapchainTextureCount = 3;
    int64_t varjoTextureFormat = 0;

    // Direct3D alpha blending used while drawing Plane pixels into the swapchain.
    bool enableAlphaBlend = true;

    // Varjo compositor layer alpha blending. Keep this enabled for MR overlays;
    // otherwise the transparent clear color can become an opaque black layer.
    bool enableLayerAlphaBlend = true;
};

class D3D11Texture final : public XRTexture {
public:
    D3D11Texture(D3D11CoreLib::D3D11Resource resource,
                 D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
                 uint32_t width,
                 uint32_t height,
                 TextureOwnership ownership);

    D3D11Texture(D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
                 uint32_t width,
                 uint32_t height);

    D3D11CoreLib::D3D11Resource& resource() noexcept { return resource_; }
    const D3D11CoreLib::D3D11Resource& resource() const noexcept { return resource_; }
    ID3D11ShaderResourceView* srv() const noexcept { return srv_.Get(); }

private:
    D3D11CoreLib::D3D11Resource resource_;
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv_;
};

class D3D11Backend final : public IRenderBackend {
public:
    D3D11Backend(std::shared_ptr<D3D11CoreLib::D3D11Core> core, D3D11BackendDesc desc = {});
    ~D3D11Backend() override;

    BackendType backendType() const noexcept override { return BackendType::D3D11; }
    void initialize(std::shared_ptr<::VarjoSession> session) override;
    void beginFrame() override;
    void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) override;
    void endFrame() override;

    std::shared_ptr<D3D11Texture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes = 0);

    std::shared_ptr<D3D11Texture> wrapTexture(
        ID3D11Texture2D* texture,
        DXGI_FORMAT srvFormat);

    std::shared_ptr<D3D11Texture> wrapSrv(
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<IRenderBackend> CreateBackend(
    std::shared_ptr<D3D11CoreLib::D3D11Core> core,
    D3D11BackendDesc desc = {});

} // namespace VarjoXR::Backends::D3D11
