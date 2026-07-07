#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>

#include <VarjoXR/Detail/VarjoLayerSubmit.hpp>
#include <VarjoXR/VarjoSession.hpp>
#include <VarjoXR/XRPlane.hpp>

#include <D3D12Helper/D3D12Core/D3D12Barrier.hpp>
#include <Varjo_d3d12.h>
#include <Varjo_layers.h>

#include <array>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace VarjoXR::Backends::D3D12 {
namespace {

struct PlaneVertex {
    float position[3];
    float uv[2];
};

struct PlaneConstants {
    float world[16];
    float view[16];
    float projection[16];
    float tint[4];
};

constexpr PlaneVertex kPlaneVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
};

constexpr uint16_t kPlaneIndices[] = {0, 1, 2, 0, 2, 3};

constexpr const char* kPlaneVertexShader = R"hlsl(
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    const float4 wp = mul(world, float4(input.position, 1.0f));
    const float4 vp = mul(view, wp);
    output.position = mul(projection, vp);
    output.uv = input.uv;
    return output;
}
)hlsl";

constexpr const char* kPlanePixelShaderPreamble = R"hlsl(
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
};
)hlsl";

constexpr const char* kPlaneDefaultPixelShader = R"hlsl(
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
};

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    return xrTexture.Sample(xrSampler, uv) * tint;
}
)hlsl";

void ThrowIfFailedLocal(HRESULT hr, const char* where) {
    if (FAILED(hr)) {
        throw std::runtime_error(std::string(where) + " failed. HRESULT=0x" + std::to_string(static_cast<unsigned long>(hr)));
    }
}

int64_t ResolveVarjoColorFormat(int64_t configured) noexcept {
    return configured == 0 ? varjo_TextureFormat_R8G8B8A8_UNORM : configured;
}

DXGI_FORMAT DxgiFormatForVarjoColorFormat(int64_t format) noexcept {
    switch (ResolveVarjoColorFormat(format)) {
    case varjo_TextureFormat_B8G8R8A8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case varjo_TextureFormat_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case varjo_TextureFormat_R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case varjo_TextureFormat_RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case varjo_TextureFormat_R8G8B8A8_UNORM:
    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

Mat4 WorldMatrixForPlane(const XRPlane& plane) {
    Transform t = plane.transform();
    t.scale.x *= plane.size().x;
    t.scale.y *= plane.size().y;
    return t.matrix();
}

std::string BuildCustomPixelShaderSource(const std::string& userMain) {
    return std::string(kPlanePixelShaderPreamble) + "\n" + userMain;
}

void CopyMat(float* dst16, const Mat4& src) noexcept {
    std::memcpy(dst16, src.m.data(), sizeof(float) * 16);
}

} // namespace

struct D3D12Backend::Impl {
    std::shared_ptr<D3D12CoreLib::D3D12Core> core;
    D3D12CoreLib::D3D12CommandContext commandContext;
    D3D12CoreLib::D3D12DescriptorAllocator cbvSrvUav;
    D3D12CoreLib::D3D12DescriptorAllocator rtvAllocator;
    Detail::VarjoLayerSubmit layer;
    varjo_SwapChain* swapChain = nullptr;
    int32_t swapChainTextureCount = 0;
    DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12CoreLib::D3D12Resource vertexBuffer;
    D3D12CoreLib::D3D12Resource indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    std::shared_ptr<D3D12Texture> whiteTexture;

    D3D12CoreLib::ComPtr<ID3D12RootSignature> rootSignature;
    std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline> defaultPipeline;
    std::unordered_map<std::string, std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline>> customPipelines;
    D3D12CoreLib::ShaderBytecode vertexShaderBytecode;

    struct SwapImageViews {
        D3D12CoreLib::D3D12Resource resource;
        std::vector<D3D12CoreLib::D3D12DescriptorHandle> rtvs;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };
    std::vector<SwapImageViews> swapImages;

    ~Impl() {
        if (core) core->WaitIdle();
        if (swapChain) {
            varjo_FreeSwapChain(swapChain);
            swapChain = nullptr;
        }
    }

    void initialize(VarjoSession& session, const XRSpaceConfig& config) {
        D3D12CoreLib::D3D12CoreConfig cfg{};
        cfg.enableDebugLayer = config.enableDebug;
        cfg.enableInfoQueue = config.enableDebug;
        cfg.createDirectQueue = true;
        cfg.createCopyQueue = true;
        cfg.createComputeQueue = false;
        core = D3D12CoreLib::D3D12Core::CreateShared(cfg);
        commandContext = core->CreateDirectContext();
        cbvSrvUav.Initialize(core->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048, true);
        rtvAllocator.Initialize(core->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false);

        layer.initialize(session.get(), config);
        auto swapConfig = layer.makeSwapChainConfig();
        swapConfig.textureFormat = ResolveVarjoColorFormat(config.colorFormat);
        renderTargetFormat = DxgiFormatForVarjoColorFormat(swapConfig.textureFormat);
        swapChainTextureCount = swapConfig.numberOfTextures;

        swapChain = varjo_D3D12CreateSwapChain(session.get(), core->GetDirectCommandQueue(), &swapConfig);
        session.throwIfError("varjo_D3D12CreateSwapChain");
        if (!swapChain) throw std::runtime_error("varjo_D3D12CreateSwapChain returned null.");
        layer.setSwapChain(swapChain);

        createSwapImageViews();
        createPlaneResources();
        createRootSignature();
        createDefaultPipeline();
        createWhiteTexture();
    }

    void createSwapImageViews() {
        swapImages.resize(static_cast<size_t>(swapChainTextureCount));
        for (int32_t image = 0; image < swapChainTextureCount; ++image) {
            const varjo_Texture varjoTexture = varjo_GetSwapChainImage(swapChain, image);
            ID3D12Resource* raw = varjo_ToD3D12Texture(varjoTexture);
            if (!raw) throw std::runtime_error("varjo_ToD3D12Texture returned null.");

            D3D12CoreLib::ComPtr<ID3D12Resource> resourcePtr = raw;
            auto& imageViews = swapImages[static_cast<size_t>(image)];
            imageViews.resource = D3D12CoreLib::D3D12Resource(resourcePtr, D3D12_RESOURCE_STATE_COMMON);
            imageViews.state = D3D12_RESOURCE_STATE_COMMON;
            imageViews.rtvs.resize(static_cast<size_t>(layer.viewCount()));

            for (int32_t view = 0; view < layer.viewCount(); ++view) {
                D3D12_RENDER_TARGET_VIEW_DESC desc{};
                desc.Format = renderTargetFormat;
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = 0;
                desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(view);
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.PlaneSlice = 0;

                auto handle = rtvAllocator.Allocate();
                D3D12CoreLib::CreateRtv(*core, imageViews.resource.Get(), desc, handle.cpu);
                imageViews.rtvs[static_cast<size_t>(view)] = handle;
            }
        }
    }

    void createPlaneResources() {
        vertexBuffer = D3D12CoreLib::CreateBuffer(
            *core,
            sizeof(kPlaneVertices),
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        void* vbMapped = nullptr;
        ThrowIfFailedLocal(vertexBuffer.Get()->Map(0, nullptr, &vbMapped), "Map plane vertex buffer");
        std::memcpy(vbMapped, kPlaneVertices, sizeof(kPlaneVertices));
        vertexBuffer.Get()->Unmap(0, nullptr);
        vertexBufferView.BufferLocation = vertexBuffer.Get()->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(kPlaneVertices));
        vertexBufferView.StrideInBytes = sizeof(PlaneVertex);

        indexBuffer = D3D12CoreLib::CreateBuffer(
            *core,
            sizeof(kPlaneIndices),
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        void* ibMapped = nullptr;
        ThrowIfFailedLocal(indexBuffer.Get()->Map(0, nullptr, &ibMapped), "Map plane index buffer");
        std::memcpy(ibMapped, kPlaneIndices, sizeof(kPlaneIndices));
        indexBuffer.Get()->Unmap(0, nullptr);
        indexBufferView.BufferLocation = indexBuffer.Get()->GetGPUVirtualAddress();
        indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(kPlaneIndices));
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }

    void createRootSignature() {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
        params[0].Descriptor.RegisterSpace = 0;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &srvRange;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = static_cast<UINT>(std::size(params));
        desc.pParameters = params;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &samplerDesc;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12CoreLib::ComPtr<ID3DBlob> blob;
        D3D12CoreLib::ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.GetAddressOf(), error.GetAddressOf());
        if (FAILED(hr)) {
            std::string message = "D3D12SerializeRootSignature failed.";
            if (error) message += std::string(" ") + static_cast<const char*>(error->GetBufferPointer());
            throw std::runtime_error(message);
        }
        ThrowIfFailedLocal(
            core->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf())),
            "CreateRootSignature");
    }

    std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline> createPipelineFromPixelSource(
        const std::string& pixelSource,
        const char* sourceName) {
        using namespace D3D12CoreLib;

        if (vertexShaderBytecode.Empty()) {
            vertexShaderBytecode = CompileShaderFromSource_D3DCompile(
                kPlaneVertexShader, "main", "vs_5_1", "VarjoXRPlaneVS.hlsl");
        }
        auto ps = CompileShaderFromSource_D3DCompile(pixelSource, "main", "ps_5_1", sourceName);

        std::vector<D3D12_INPUT_ELEMENT_DESC> layout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        auto rasterizer = PipelineDefaults::Rasterizer(D3D12_CULL_MODE_NONE);
        auto blend = PipelineDefaults::BlendAlpha();
        auto depth = PipelineDefaults::DepthDisabled();

        GraphicsPipelineDesc desc{};
        desc.vs = vertexShaderBytecode;
        desc.ps = ps;
        desc.inputLayout = layout;
        desc.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.numRenderTargets = 1;
        desc.rtvFormats[0] = renderTargetFormat;
        desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
        desc.sampleCount = 1;
        desc.sampleQuality = 0;
        desc.rasterizer = &rasterizer;
        desc.blend = &blend;
        desc.depthStencil = &depth;

        auto pipeline = std::make_unique<D3D12GraphicsPipeline>();
        pipeline->Initialize(core->GetDevice(), rootSignature, desc);
        return pipeline;
    }

    void createDefaultPipeline() {
        defaultPipeline = createPipelineFromPixelSource(kPlaneDefaultPixelShader, "VarjoXRPlaneDefaultPS.hlsl");
    }

    D3D12CoreLib::D3D12GraphicsPipeline& pipelineForMaterial(const Material& material) {
        if (material.pixelShaderHlsl.empty()) return *defaultPipeline;

        auto found = customPipelines.find(material.pixelShaderHlsl);
        if (found != customPipelines.end()) return *found->second;

        try {
            auto pipeline = createPipelineFromPixelSource(
                BuildCustomPixelShaderSource(material.pixelShaderHlsl),
                "VarjoXRCustomPlanePS.hlsl");
            auto* raw = pipeline.get();
            customPipelines.emplace(material.pixelShaderHlsl, std::move(pipeline));
            return *raw;
        } catch (const std::exception& e) {
            OutputDebugStringA("VarjoXR D3D12 custom pixel shader compile failed. Falling back to default shader.\n");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
            return *defaultPipeline;
        }
    }

    void createWhiteTexture() {
        const uint8_t white[4] = {255, 255, 255, 255};
        whiteTexture = std::static_pointer_cast<D3D12Texture>(createTextureFromRGBA(white, 1, 1, 4));
    }

    void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) {
        layer.waitSync(session.get());
        varjo_BeginFrameWithLayers(session.get());
        session.throwIfError("varjo_BeginFrameWithLayers");

        int32_t imageIndex = -1;
        varjo_AcquireSwapChainImage(swapChain, &imageIndex);
        session.throwIfError("varjo_AcquireSwapChainImage");
        if (imageIndex < 0 || imageIndex >= swapChainTextureCount) {
            throw std::runtime_error("varjo_AcquireSwapChainImage returned invalid image index.");
        }

        renderToSwapImage(imageIndex, objects);

        varjo_ReleaseSwapChainImage(swapChain);
        session.throwIfError("varjo_ReleaseSwapChainImage");

        auto submitInfo = layer.makeSubmitInfo();
        varjo_EndFrameWithLayers(session.get(), &submitInfo);
        session.throwIfError("varjo_EndFrameWithLayers");
    }

    void renderToSwapImage(int32_t imageIndex, const std::vector<std::unique_ptr<XRObject>>& objects) {
        auto& imageViews = swapImages[static_cast<size_t>(imageIndex)];
        commandContext.Reset();
        auto* cmd = commandContext.GetCommandList();

        if (imageViews.state != D3D12_RESOURCE_STATE_RENDER_TARGET) {
            auto barrier = D3D12CoreLib::MakeTransitionBarrier(
                imageViews.resource.Get(), imageViews.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandContext.ResourceBarrier(barrier);
            imageViews.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }

        ID3D12DescriptorHeap* heaps[] = {cbvSrvUav.GetHeap()};
        cmd->SetDescriptorHeaps(1, heaps);

        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        std::vector<D3D12CoreLib::D3D12Resource> perDrawConstants;
        perDrawConstants.reserve(objects.size() * static_cast<size_t>(std::max(1, layer.viewCount())));

        for (int32_t view = 0; view < layer.viewCount(); ++view) {
            const auto& viewState = layer.viewState(view);
            auto rtv = imageViews.rtvs[static_cast<size_t>(view)].cpu;
            cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
            cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            D3D12_VIEWPORT viewport{};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(viewState.preferredWidth);
            viewport.Height = static_cast<float>(viewState.preferredHeight);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            D3D12_RECT scissor{};
            scissor.left = 0;
            scissor.top = 0;
            scissor.right = viewState.preferredWidth;
            scissor.bottom = viewState.preferredHeight;
            cmd->RSSetViewports(1, &viewport);
            cmd->RSSetScissorRects(1, &scissor);

            for (const auto& object : objects) {
                if (!object || object->kind() != XRObjectKind::Plane) continue;
                renderPlane(viewState, static_cast<const XRPlane&>(*object), perDrawConstants);
            }
        }

        if (imageViews.state != D3D12_RESOURCE_STATE_COMMON) {
            auto barrier = D3D12CoreLib::MakeTransitionBarrier(
                imageViews.resource.Get(), imageViews.state, D3D12_RESOURCE_STATE_COMMON);
            commandContext.ResourceBarrier(barrier);
            imageViews.state = D3D12_RESOURCE_STATE_COMMON;
        }

        commandContext.Close();
        ID3D12CommandList* lists[] = {cmd};
        core->DirectQueue().ExecuteCommandLists(1, lists);
        core->DirectQueue().WaitIdle();
    }

    void renderPlane(
        const Detail::VarjoViewState& viewState,
        const XRPlane& plane,
        std::vector<D3D12CoreLib::D3D12Resource>& perDrawConstants) {
        auto* cmd = commandContext.GetCommandList();
        const auto& material = plane.materialFor(viewState.eye);
        auto& pipeline = pipelineForMaterial(material);
        pipeline.Bind(commandContext);

        auto cb = D3D12CoreLib::CreateConstantBuffer(*core, sizeof(PlaneConstants));
        PlaneConstants constants{};
        CopyMat(constants.world, WorldMatrixForPlane(plane));
        CopyMat(constants.view, viewState.view);
        CopyMat(constants.projection, viewState.projection);
        constants.tint[0] = material.tint.x;
        constants.tint[1] = material.tint.y;
        constants.tint[2] = material.tint.z;
        constants.tint[3] = material.tint.w;

        void* mapped = nullptr;
        ThrowIfFailedLocal(cb.Get()->Map(0, nullptr, &mapped), "Map plane constant buffer");
        std::memcpy(mapped, &constants, sizeof(constants));
        cb.Get()->Unmap(0, nullptr);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmd->IASetIndexBuffer(&indexBufferView);
        cmd->SetGraphicsRootConstantBufferView(0, cb.Get()->GetGPUVirtualAddress());

        auto texture = std::dynamic_pointer_cast<D3D12Texture>(material.texture);
        if (!texture || !texture->srv.IsValid() || texture->srv.gpu.ptr == 0) texture = whiteTexture;
        if (!texture || !texture->srv.IsValid() || texture->srv.gpu.ptr == 0) return;
        cmd->SetGraphicsRootDescriptorTable(1, texture->srv.gpu);
        cmd->DrawIndexedInstanced(static_cast<UINT>(std::size(kPlaneIndices)), 1, 0, 0, 0);

        perDrawConstants.emplace_back(std::move(cb));
    }

    std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) {
        if (!core) throw std::runtime_error("D3D12Backend is not initialized.");
        if (!rgba || width == 0 || height == 0) throw std::runtime_error("Invalid RGBA texture input.");
        auto res = D3D12CoreLib::CreateTexture2DFromMemory(
            *core, rgba, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitchBytes,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        auto handle = cbvSrvUav.Allocate();
        D3D12CoreLib::CreateTexture2DSrv(*core, res, handle.cpu);
        return std::make_shared<D3D12Texture>(std::move(res), handle, width, height);
    }

    std::shared_ptr<XRTexture> createTextureFromD3D12Resource(
        ID3D12Resource* resource,
        DXGI_FORMAT srvFormat) {
        if (!core) throw std::runtime_error("D3D12Backend is not initialized.");
        if (!resource) throw std::runtime_error("createTextureFromD3D12Resource: resource is null.");
        D3D12CoreLib::ComPtr<ID3D12Resource> resourcePtr = resource;
        D3D12CoreLib::D3D12Resource wrapped(resourcePtr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const auto desc = wrapped.GetDesc();
        auto handle = cbvSrvUav.Allocate();
        D3D12CoreLib::CreateTexture2DSrv(*core, wrapped, handle.cpu, srvFormat);
        return std::make_shared<D3D12Texture>(std::move(wrapped), handle,
            static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height));
    }
};

D3D12Backend::D3D12Backend() : m_impl(std::make_unique<Impl>()) {}
D3D12Backend::~D3D12Backend() = default;

void D3D12Backend::initialize(VarjoSession& session, const XRSpaceConfig& config) { m_impl->initialize(session, config); }
void D3D12Backend::renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) { m_impl->renderFrame(session, objects); }
std::shared_ptr<XRTexture> D3D12Backend::createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
    return m_impl->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}
std::shared_ptr<XRTexture> D3D12Backend::createTextureFromD3D12Resource(ID3D12Resource* resource, DXGI_FORMAT srvFormat) {
    return m_impl->createTextureFromD3D12Resource(resource, srvFormat);
}

} // namespace VarjoXR::Backends::D3D12
