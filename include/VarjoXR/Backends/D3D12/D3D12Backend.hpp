#pragma once

#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Core/ThrowIfFailed.hpp>
#include <D3D12Helper/D3D12Gpu/D3D12Gpu.hpp>
#include <D3D12Helper/D3D12Processing/D3D12Processing.hpp>

#include <VarjoXR/Backends/IRenderBackend.hpp>
#include <VarjoXR/Core/XRTexture.hpp>

struct ID3D12Resource;

typedef enum DXGI_FORMAT DXGI_FORMAT;

namespace VarjoXR::Backends::D3D12 {

struct D3D12BackendDesc {
    int32_t swapchainTextureCount = 3;
    int64_t varjoTextureFormat = 0;

    // Direct3D alpha blending used while drawing Plane pixels into the swapchain.
    bool enableAlphaBlend = true;

    // Varjo compositor layer alpha blending. Keep this enabled for MR overlays;
    // otherwise the transparent clear color can become an opaque black layer.
    bool enableLayerAlphaBlend = true;

    uint32_t cbvSrvUavDescriptorCount = 2048;
    uint32_t rtvDescriptorCount = 256;

    // Number of CPU/GPU frame slots used for command allocators and upload buffers.
    // Must be at least 2 for non-blocking D3D12 submission; values below 2 are clamped.
    uint32_t frameResourceCount = 3;
};

class D3D12Texture final : public XRTexture {
public:
    D3D12Texture(D3D12CoreLib::D3D12Resource resource,
                 D3D12CoreLib::D3D12DescriptorHandle srv,
                 uint32_t width,
                 uint32_t height,
                 TextureOwnership ownership);

    D3D12CoreLib::D3D12Resource& resource() noexcept { return resource_; }
    const D3D12CoreLib::D3D12Resource& resource() const noexcept { return resource_; }
    const D3D12CoreLib::D3D12DescriptorHandle& srv() const noexcept { return srv_; }

private:
    D3D12CoreLib::D3D12Resource resource_;
    D3D12CoreLib::D3D12DescriptorHandle srv_{};
};

class D3D12Backend final : public IRenderBackend {
public:
    D3D12Backend(std::shared_ptr<D3D12CoreLib::D3D12Core> core, D3D12BackendDesc desc = {});
    ~D3D12Backend() override;

    BackendType backendType() const noexcept override { return BackendType::D3D12; }
    void initialize(std::shared_ptr<::VarjoSession> session) override;
    void beginFrame() override;
    void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) override;
    void endFrame() override;

    std::shared_ptr<D3D12Texture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes = 0);

    std::shared_ptr<D3D12Texture> wrapResource(
        ID3D12Resource* resource,
        DXGI_FORMAT srvFormat);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<IRenderBackend> CreateBackend(
    std::shared_ptr<D3D12CoreLib::D3D12Core> core,
    D3D12BackendDesc desc = {});

} // namespace VarjoXR::Backends::D3D12
