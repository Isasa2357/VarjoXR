#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>

#include <VarjoXR/Detail/VarjoLayerSubmit.hpp>
#include <VarjoXR/VarjoSession.hpp>
#include <VarjoXR/XRPlane.hpp>

#include <Varjo_d3d11.h>
#include <Varjo_layers.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace VarjoXR::Backends::D3D11 {
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

struct D3D11Backend::Impl {
    std::shared_ptr<D3D11CoreLib::D3D11Core> core;
    Detail::VarjoLayerSubmit layer;
    varjo_SwapChain* swapChain = nullptr;
    int32_t swapChainTextureCount = 0;
    DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D11CoreLib::D3D11Resource vertexBuffer;
    D3D11CoreLib::D3D11Resource indexBuffer;
    D3D11CoreLib::D3D11Resource constantBuffer;
    D3D11CoreLib::ComPtr<ID3D11SamplerState> sampler;
    std::shared_ptr<D3D11Texture> whiteTexture;

    std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline> defaultPipeline;
    std::unordered_map<std::string, std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline>> customPipelines;
    D3D11CoreLib::ShaderBytecode vertexShaderBytecode;

    struct SwapImageViews {
        D3D11CoreLib::ComPtr<ID3D11Texture2D> texture;
        std::vector<D3D11CoreLib::ComPtr<ID3D11RenderTargetView>> rtvs;
    };
    std::vector<SwapImageViews> swapImages;

    ~Impl() {
        if (core) core->Flush();
        if (swapChain) {
            varjo_FreeSwapChain(swapChain);
            swapChain = nullptr;
        }
    }

    void initialize(VarjoSession& session, const XRSpaceConfig& config) {
        D3D11CoreLib::D3D11CoreConfig cfg{};
        cfg.enableDebugLayer = config.enableDebug;
        cfg.enableInfoQueue = config.enableDebug;
        cfg.enableMultithreadProtection = true;
        core = D3D11CoreLib::D3D11Core::CreateShared(cfg);

        layer.initialize(session.get(), config);
        auto swapConfig = layer.makeSwapChainConfig();
        swapConfig.textureFormat = ResolveVarjoColorFormat(config.colorFormat);
        renderTargetFormat = DxgiFormatForVarjoColorFormat(swapConfig.textureFormat);
        swapChainTextureCount = swapConfig.numberOfTextures;

        swapChain = varjo_D3D11CreateSwapChain(session.get(), core->GetDevice(), &swapConfig);
        session.throwIfError("varjo_D3D11CreateSwapChain");
        if (!swapChain) throw std::runtime_error("varjo_D3D11CreateSwapChain returned null.");
        layer.setSwapChain(swapChain);

        createSwapImageViews();
        createPlaneResources();
        createDefaultPipeline();
        createWhiteTexture();
    }

    void createSwapImageViews() {
        swapImages.resize(static_cast<size_t>(swapChainTextureCount));
        for (int32_t image = 0; image < swapChainTextureCount; ++image) {
            const varjo_Texture varjoTexture = varjo_GetSwapChainImage(swapChain, image);
            ID3D11Texture2D* raw = varjo_ToD3D11Texture(varjoTexture);
            if (!raw) throw std::runtime_error("varjo_ToD3D11Texture returned null.");

            auto& imageViews = swapImages[static_cast<size_t>(image)];
            imageViews.texture = raw;
            imageViews.rtvs.resize(static_cast<size_t>(layer.viewCount()));

            for (int32_t view = 0; view < layer.viewCount(); ++view) {
                D3D11_RENDER_TARGET_VIEW_DESC desc{};
                desc.Format = renderTargetFormat;
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = 0;
                desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(view);
                desc.Texture2DArray.ArraySize = 1;
                imageViews.rtvs[static_cast<size_t>(view)] =
                    D3D11CoreLib::CreateRtv(*core, raw, desc);
            }
        }
    }

    void createPlaneResources() {
        vertexBuffer = D3D11CoreLib::CreateBuffer(
            *core,
            static_cast<UINT>(sizeof(kPlaneVertices)),
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_VERTEX_BUFFER,
            0,
            0,
            kPlaneVertices);

        indexBuffer = D3D11CoreLib::CreateBuffer(
            *core,
            static_cast<UINT>(sizeof(kPlaneIndices)),
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_INDEX_BUFFER,
            0,
            0,
            kPlaneIndices);

        constantBuffer = D3D11CoreLib::CreateConstantBuffer(*core, sizeof(PlaneConstants));
        sampler = D3D11CoreLib::CreateSampler(*core, D3D11CoreLib::MakeLinearClampSamplerDesc());
    }

    std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline> createPipelineFromPixelSource(
        const std::string& pixelSource,
        const char* sourceName) {
        using namespace D3D11CoreLib;

        if (vertexShaderBytecode.Empty()) {
            vertexShaderBytecode = CompileShaderFromSource_D3DCompile(
                kPlaneVertexShader, "main", "vs_5_0", "VarjoXRPlaneVS.hlsl");
        }
        auto ps = CompileShaderFromSource_D3DCompile(pixelSource, "main", "ps_5_0", sourceName);

        std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        auto rasterizer = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        auto blend = PipelineDefaults::BlendAlpha();
        auto depth = PipelineDefaults::DepthDisabled();

        GraphicsPipelineDesc desc{};
        desc.vs = vertexShaderBytecode;
        desc.ps = ps;
        desc.inputLayout = layout;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        desc.rasterizer = &rasterizer;
        desc.blend = &blend;
        desc.depthStencil = &depth;

        auto pipeline = std::make_unique<D3D11GraphicsPipeline>();
        pipeline->Initialize(core->GetDevice(), desc);
        return pipeline;
    }

    void createDefaultPipeline() {
        defaultPipeline = createPipelineFromPixelSource(kPlaneDefaultPixelShader, "VarjoXRPlaneDefaultPS.hlsl");
    }

    D3D11CoreLib::D3D11GraphicsPipeline& pipelineForMaterial(const Material& material) {
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
            OutputDebugStringA("VarjoXR D3D11 custom pixel shader compile failed. Falling back to default shader.\n");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
            return *defaultPipeline;
        }
    }

    void createWhiteTexture() {
        const uint8_t white[4] = {255, 255, 255, 255};
        whiteTexture = std::static_pointer_cast<D3D11Texture>(createTextureFromRGBA(white, 1, 1, 4));
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
        auto* ctx = core->GetImmediateContext();
        auto& imageViews = swapImages[static_cast<size_t>(imageIndex)];
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        for (int32_t view = 0; view < layer.viewCount(); ++view) {
            auto rtv = imageViews.rtvs[static_cast<size_t>(view)].Get();
            ctx->ClearRenderTargetView(rtv, clear);
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            const auto& viewState = layer.viewState(view);
            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(viewState.preferredWidth);
            viewport.Height = static_cast<float>(viewState.preferredHeight);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &viewport);

            for (const auto& object : objects) {
                if (!object || object->kind() != XRObjectKind::Plane) continue;
                renderPlane(viewState, static_cast<const XRPlane&>(*object));
            }
        }

        ID3D11RenderTargetView* nullRtv = nullptr;
        ctx->OMSetRenderTargets(1, &nullRtv, nullptr);
        core->Flush();
    }

    void renderPlane(const Detail::VarjoViewState& viewState, const XRPlane& plane) {
        auto* ctx = core->GetImmediateContext();
        const auto& material = plane.materialFor(viewState.eye);
        auto& pipeline = pipelineForMaterial(material);
        pipeline.Bind(ctx);

        PlaneConstants constants{};
        CopyMat(constants.world, WorldMatrixForPlane(plane));
        CopyMat(constants.view, viewState.view);
        CopyMat(constants.projection, viewState.projection);
        constants.tint[0] = material.tint.x;
        constants.tint[1] = material.tint.y;
        constants.tint[2] = material.tint.z;
        constants.tint[3] = material.tint.w;
        ctx->UpdateSubresource(constantBuffer.AsBuffer(), 0, nullptr, &constants, 0, 0);

        ID3D11Buffer* cb = constantBuffer.AsBuffer();
        ctx->VSSetConstantBuffers(0, 1, &cb);
        ctx->PSSetConstantBuffers(0, 1, &cb);

        UINT stride = sizeof(PlaneVertex);
        UINT offset = 0;
        ID3D11Buffer* vb = vertexBuffer.AsBuffer();
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(indexBuffer.AsBuffer(), DXGI_FORMAT_R16_UINT, 0);

        auto texture = std::dynamic_pointer_cast<D3D11Texture>(material.texture);
        if (!texture || !texture->srv) texture = whiteTexture;
        ID3D11ShaderResourceView* srv = texture ? texture->srv.Get() : nullptr;
        ctx->PSSetShaderResources(0, 1, &srv);
        ID3D11SamplerState* s = sampler.Get();
        ctx->PSSetSamplers(0, 1, &s);

        ctx->DrawIndexed(static_cast<UINT>(std::size(kPlaneIndices)), 0, 0);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        ctx->PSSetShaderResources(0, 1, &nullSrv);
    }

    std::shared_ptr<XRTexture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) {
        if (!core) throw std::runtime_error("D3D11Backend is not initialized.");
        if (!rgba || width == 0 || height == 0) throw std::runtime_error("Invalid RGBA texture input.");
        auto res = D3D11CoreLib::CreateTexture2DFromMemory(
            *core, rgba, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitchBytes);
        auto srv = D3D11CoreLib::CreateTexture2DSrv(*core, res);
        return std::make_shared<D3D11Texture>(std::move(res), std::move(srv), width, height);
    }

    std::shared_ptr<XRTexture> createTextureFromD3D11Resource(
        ID3D11Texture2D* texture,
        DXGI_FORMAT srvFormat) {
        if (!core) throw std::runtime_error("D3D11Backend is not initialized.");
        if (!texture) throw std::runtime_error("createTextureFromD3D11Resource: texture is null.");

        D3D11CoreLib::ComPtr<ID3D11Texture2D> tex = texture;
        D3D11CoreLib::D3D11Resource resource(tex);
        auto srv = D3D11CoreLib::CreateTexture2DSrv(*core, resource, srvFormat);
        const auto desc = resource.GetTexture2DDesc();
        return std::make_shared<D3D11Texture>(std::move(resource), std::move(srv), desc.Width, desc.Height);
    }

    std::shared_ptr<XRTexture> createTextureFromD3D11Srv(
        ID3D11ShaderResourceView* srv,
        uint32_t width,
        uint32_t height) {
        if (!srv) throw std::runtime_error("createTextureFromD3D11Srv: srv is null.");
        D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srvPtr = srv;
        return std::make_shared<D3D11Texture>(std::move(srvPtr), width, height);
    }
};

D3D11Backend::D3D11Backend() : m_impl(std::make_unique<Impl>()) {}
D3D11Backend::~D3D11Backend() = default;

void D3D11Backend::initialize(VarjoSession& session, const XRSpaceConfig& config) { m_impl->initialize(session, config); }
void D3D11Backend::renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) { m_impl->renderFrame(session, objects); }
std::shared_ptr<XRTexture> D3D11Backend::createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
    return m_impl->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}
std::shared_ptr<XRTexture> D3D11Backend::createTextureFromD3D11Resource(ID3D11Texture2D* texture, DXGI_FORMAT srvFormat) {
    return m_impl->createTextureFromD3D11Resource(texture, srvFormat);
}
std::shared_ptr<XRTexture> D3D11Backend::createTextureFromD3D11Srv(ID3D11ShaderResourceView* srv, uint32_t width, uint32_t height) {
    return m_impl->createTextureFromD3D11Srv(srv, width, height);
}

} // namespace VarjoXR::Backends::D3D11
