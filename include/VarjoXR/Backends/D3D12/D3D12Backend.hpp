#pragma once

#include <VarjoXR/Detail/IRenderBackend.hpp>

#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Framework.hpp>

#include <utility>

namespace VarjoXR::Backends::D3D12 {

class D3D12Texture final : public XRTexture {
public:
    D3D12Texture(D3D12CoreLib::D3D12Resource resource,
                 D3D12CoreLib::D3D12DescriptorHandle srv,
                 uint32_t width,
                 uint32_t height)
        : XRTexture(BackendType::D3D12, width, height), resource(std::move(resource)), srv(srv) {}

    D3D12CoreLib::D3D12Resource resource;
    D3D12CoreLib::D3D12DescriptorHandle srv;
};

class D3D12Backend final : public IRenderBackend {
public:
    D3D12Backend();
    ~D3D12Backend() override;

    BackendType type() const noexcept override { return BackendType::D3D12; }
    void initialize(VarjoSession& session, const XRSpaceConfig& config) override;
    void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) override;
    std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace VarjoXR::Backends::D3D12
