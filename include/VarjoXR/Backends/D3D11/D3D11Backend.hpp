#pragma once

#include <VarjoXR/Detail/IRenderBackend.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>

#include <utility>

namespace VarjoXR::Backends::D3D11 {

class D3D11Texture final : public XRTexture {
public:
    D3D11Texture(D3D11CoreLib::D3D11Resource resource,
                 D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
                 uint32_t width,
                 uint32_t height)
        : XRTexture(BackendType::D3D11, width, height), resource(std::move(resource)), srv(std::move(srv)) {}

    D3D11Texture(D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
                 uint32_t width,
                 uint32_t height)
        : XRTexture(BackendType::D3D11, width, height), srv(std::move(srv)) {}

    D3D11CoreLib::D3D11Resource resource;
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv;
};

class D3D11Backend final : public IRenderBackend {
public:
    D3D11Backend();
    ~D3D11Backend() override;

    BackendType type() const noexcept override { return BackendType::D3D11; }
    void initialize(VarjoSession& session, const XRSpaceConfig& config) override;
    void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) override;
    std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) override;
    std::shared_ptr<XRTexture> createTextureFromD3D11Resource(
        ID3D11Texture2D* texture,
        DXGI_FORMAT srvFormat) override;
    std::shared_ptr<XRTexture> createTextureFromD3D11Srv(
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace VarjoXR::Backends::D3D11
