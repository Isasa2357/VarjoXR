#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>

#include <VarjoXR/Core/XRPlane.hpp>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d11.h>
#include <Varjo_layers.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <windows.h>

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
    float params0[4];
    float params1[4];
    float frameParams[4]; // gazeUv.xy, timeSeconds, frameNumber modulo float range
};

constexpr PlaneVertex kPlaneVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
};

constexpr uint16_t kPlaneIndices[] = {0, 1, 2, 0, 2, 3};
constexpr UINT kPlaneIndexCount = static_cast<UINT>(sizeof(kPlaneIndices) / sizeof(kPlaneIndices[0]));

constexpr const char* kPlaneVertexShader = R"hlsl(
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
    float4 params0;
    float4 params1;
    float4 frameParams;
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
    float4 worldPos = mul(world, float4(input.position, 1.0f));
    float4 viewPos = mul(view, worldPos);
    output.position = mul(projection, viewPos);
    output.uv = input.uv;
    return output;
}
)hlsl";

constexpr const char* kPlaneShaderPreamble = R"hlsl(
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
    float4 params0;
    float4 params1;
    float4 frameParams;
};
)hlsl";

constexpr const char* kDefaultPlanePixelShader = R"hlsl(
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
    float4 params0;
    float4 params1;
    float4 frameParams;
};

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    return xrTexture.Sample(xrSampler, uv) * tint;
}
)hlsl";

varjo_TextureFormat ResolveVarjoTextureFormat(int64_t configured) noexcept {
    if (configured == 0) {
        return varjo_TextureFormat_R8G8B8A8_UNORM;
    }
    return static_cast<varjo_TextureFormat>(configured);
}

DXGI_FORMAT ResolveDxgiFormat(varjo_TextureFormat format) noexcept {
    switch (format) {
    case varjo_TextureFormat_B8G8R8A8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case varjo_TextureFormat_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case varjo_TextureFormat_R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case varjo_TextureFormat_RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case varjo_TextureFormat_R8G8B8A8_UNORM:
    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

Eye EyeFromDescription(const varjo_ViewDescription& desc) noexcept {
    return desc.eye == varjo_Eye_Right ? Eye::Right : Eye::Left;
}

glm::mat4 Mat4FromVarjo(const varjo_Matrix& matrix) noexcept {
    glm::mat4 out(1.0f);
    for (int i = 0; i < 16; ++i) {
        glm::value_ptr(out)[i] = static_cast<float>(matrix.value[i]);
    }
    return out;
}

void CopyMatrix(float* dst, const glm::mat4& matrix) noexcept {
    std::memcpy(dst, glm::value_ptr(matrix), sizeof(float) * 16);
}

void CopyVec4(float* dst, const glm::vec4& value) noexcept {
    dst[0] = value.x;
    dst[1] = value.y;
    dst[2] = value.z;
    dst[3] = value.w;
}

glm::mat4 PlaneLocalMatrix(const XRPlane& plane) {
    Transform transform = plane.transform();
    transform.scale.x *= plane.size().x;
    transform.scale.y *= plane.size().y;
    return transform.matrix();
}

std::string BuildUserPixelShader(const std::string& userHlsl) {
    if (userHlsl.empty()) {
        return kDefaultPlanePixelShader;
    }
    return std::string(kPlaneShaderPreamble) + "\n" + userHlsl;
}

} // namespace

D3D11Texture::D3D11Texture(
    D3D11CoreLib::D3D11Resource resource,
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
    uint32_t width,
    uint32_t height,
    TextureOwnership ownership)
    : XRTexture(BackendType::D3D11, width, height, ownership)
    , resource_(std::move(resource))
    , srv_(std::move(srv)) {}

D3D11Texture::D3D11Texture(
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
    uint32_t width,
    uint32_t height)
    : XRTexture(BackendType::D3D11, width, height, TextureOwnership::External)
    , srv_(std::move(srv)) {}

struct D3D11Backend::Impl {
    std::shared_ptr<D3D11CoreLib::D3D11Core> core;
    D3D11BackendDesc desc{};
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<::VarjoFrameInfo> frameInfo;
    std::unique_ptr<::VarjoLayerFrame> layerFrame;
    std::unique_ptr<::VarjoMultiProjLayer> multiProjLayer;
    std::unique_ptr<::VarjoSwapChain> swapChain;
    bool frameBegun = false;
    bool acquired = false;
    int32_t acquiredImageIndex = -1;
    int32_t viewCount = 0;
    int32_t swapchainWidth = 1;
    int32_t swapchainHeight = 1;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    std::vector<varjo_ViewDescription> viewDescriptions;

    D3D11CoreLib::D3D11Resource vertexBuffer;
    D3D11CoreLib::D3D11Resource indexBuffer;
    D3D11CoreLib::D3D11Resource constantBuffer;
    D3D11CoreLib::ComPtr<ID3D11SamplerState> sampler;
    std::shared_ptr<D3D11Texture> whiteTexture;

    D3D11CoreLib::ShaderBytecode vertexShaderBytecode;
    std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline> defaultPipeline;
    std::unordered_map<std::string, std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline>> pipelineCache;

    struct SwapImageViews {
        D3D11CoreLib::ComPtr<ID3D11Texture2D> texture;
        std::vector<D3D11CoreLib::ComPtr<ID3D11RenderTargetView>> rtvs;
    };
    std::vector<SwapImageViews> swapImages;

    explicit Impl(std::shared_ptr<D3D11CoreLib::D3D11Core> inCore, D3D11BackendDesc inDesc)
        : core(std::move(inCore)), desc(inDesc) {
        if (!core) {
            throw std::runtime_error("D3D11Backend requires an externally-created D3D11Core shared_ptr.");
        }
    }

    void initialize(std::shared_ptr<::VarjoSession> inSession) {
        if (!inSession || !inSession->valid()) {
            throw std::runtime_error("D3D11Backend::initialize requires a valid external VarjoSession.");
        }
        session = std::move(inSession);
        frameInfo = std::make_unique<::VarjoFrameInfo>(*session);
        layerFrame = std::make_unique<::VarjoLayerFrame>(*session);
        if (!frameInfo || !frameInfo->valid()) {
            throw std::runtime_error("D3D11Backend failed to create VarjoFrameInfo.");
        }
        if (!layerFrame || !layerFrame->valid()) {
            throw std::runtime_error("D3D11Backend failed to create VarjoLayerFrame.");
        }

        createSwapChain();
        createPlaneResources();
        createWhiteTexture();
        createDefaultPipeline();

        OutputDebugStringA("[VarjoXR][D3D11] Backend initialized with external D3D11Core and external VarjoSession.\n");
    }

    void createSwapChain() {
        viewCount = std::max(1, session->viewCount());
        viewDescriptions.resize(static_cast<size_t>(viewCount));
        swapchainWidth = 1;
        swapchainHeight = 1;
        for (int32_t i = 0; i < viewCount; ++i) {
            viewDescriptions[static_cast<size_t>(i)] = varjo_GetViewDescription(session->get(), i);
            swapchainWidth = std::max(swapchainWidth, viewDescriptions[static_cast<size_t>(i)].width);
            swapchainHeight = std::max(swapchainHeight, viewDescriptions[static_cast<size_t>(i)].height);
        }

        const auto format = ResolveVarjoTextureFormat(desc.varjoTextureFormat);
        rtvFormat = ResolveDxgiFormat(format);
        auto config = VarjoSwapChain::makeConfig(
            format,
            swapchainWidth,
            swapchainHeight,
            std::max(2, desc.swapchainTextureCount),
            viewCount);

        auto chain = VarjoSwapChain::createD3D11(session->shared(), core->GetDevice(), config);
        if (!chain.valid()) {
            throw std::runtime_error("D3D11Backend failed to create VarjoSwapChain: " + chain.lastError());
        }
        swapChain = std::make_unique<VarjoSwapChain>(std::move(chain));
        multiProjLayer = std::make_unique<VarjoMultiProjLayer>(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);

        createSwapImageViews();
    }

    void createSwapImageViews() {
        swapImages.resize(static_cast<size_t>(swapChain->config().numberOfTextures));
        for (int32_t imageIndex = 0; imageIndex < swapChain->config().numberOfTextures; ++imageIndex) {
            const varjo_Texture varjoTexture = swapChain->image(imageIndex);
            ID3D11Texture2D* rawTexture = varjo_ToD3D11Texture(varjoTexture);
            if (!rawTexture) {
                throw std::runtime_error("D3D11Backend: varjo_ToD3D11Texture returned null.");
            }

            auto& image = swapImages[static_cast<size_t>(imageIndex)];
            image.texture = rawTexture;
            image.rtvs.resize(static_cast<size_t>(viewCount));

            for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = rtvFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(viewIndex);
                rtvDesc.Texture2DArray.ArraySize = 1;
                image.rtvs[static_cast<size_t>(viewIndex)] = D3D11CoreLib::CreateRtv(*core, rawTexture, rtvDesc);
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

    void createWhiteTexture() {
        const uint8_t white[4] = {255, 255, 255, 255};
        whiteTexture = createTextureFromRGBA(white, 1, 1, 4);
    }

    void createDefaultPipeline() {
        defaultPipeline = createPipeline(kDefaultPlanePixelShader, "VarjoXR_D3D11_DefaultPlanePS.hlsl");
    }

    std::unique_ptr<D3D11CoreLib::D3D11GraphicsPipeline> createPipeline(
        const std::string& pixelShaderSource,
        const std::string& sourceName) {
        if (vertexShaderBytecode.Empty()) {
            vertexShaderBytecode = D3D11CoreLib::CompileShaderFromSource_D3DCompile(
                kPlaneVertexShader,
                "main",
                "vs_5_0",
                "VarjoXR_D3D11_PlaneVS.hlsl");
        }

        auto pixelBytecode = D3D11CoreLib::CompileShaderFromSource_D3DCompile(
            pixelShaderSource,
            "main",
            "ps_5_0",
            sourceName);

        std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        auto rasterizer = D3D11CoreLib::PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        auto blend = desc.enableAlphaBlend ? D3D11CoreLib::PipelineDefaults::BlendAlpha()
                                           : D3D11CoreLib::PipelineDefaults::BlendOpaque();
        auto depth = D3D11CoreLib::PipelineDefaults::DepthDisabled();

        D3D11CoreLib::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.vs = vertexShaderBytecode;
        pipelineDesc.ps = pixelBytecode;
        pipelineDesc.inputLayout = inputLayout;
        pipelineDesc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        pipelineDesc.rasterizer = &rasterizer;
        pipelineDesc.blend = &blend;
        pipelineDesc.depthStencil = &depth;

        auto pipeline = std::make_unique<D3D11CoreLib::D3D11GraphicsPipeline>();
        pipeline->Initialize(core->GetDevice(), pipelineDesc);
        return pipeline;
    }

    D3D11CoreLib::D3D11GraphicsPipeline& pipelineFor(const XRMaterial& material) {
        if (material.planePixelShaderHlsl.empty()) {
            return *defaultPipeline;
        }
        auto it = pipelineCache.find(material.planePixelShaderHlsl);
        if (it != pipelineCache.end()) {
            return *it->second;
        }
        try {
            auto pipeline = createPipeline(BuildUserPixelShader(material.planePixelShaderHlsl), "VarjoXR_D3D11_UserPlanePS.hlsl");
            auto* raw = pipeline.get();
            pipelineCache.emplace(material.planePixelShaderHlsl, std::move(pipeline));
            return *raw;
        } catch (const std::exception& e) {
            OutputDebugStringA("[VarjoXR][D3D11] Plane HLSL compile failed. Falling back to default shader.\n");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
            return *defaultPipeline;
        }
    }

    void beginFrame() {
        if (frameBegun) {
            throw std::runtime_error("D3D11Backend::beginFrame called while a frame is already active.");
        }
        if (!frameInfo->waitSync()) {
            throw std::runtime_error("D3D11Backend::beginFrame: VarjoFrameInfo::waitSync failed: " + session->lastError());
        }
        if (!layerFrame->begin()) {
            throw std::runtime_error("D3D11Backend::beginFrame: VarjoLayerFrame::begin failed: " + layerFrame->lastError());
        }
        if (!swapChain->acquire(acquiredImageIndex)) {
            throw std::runtime_error("D3D11Backend::beginFrame: VarjoSwapChain::acquire failed: " + swapChain->lastError());
        }
        acquired = true;
        frameBegun = true;
    }

    glm::mat4 computeHeadMatrix() const {
        glm::mat4 head(1.0f);
        glm::vec3 positionSum(0.0f);
        int count = 0;
        bool rotationSet = false;
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);

        for (int32_t i = 0; i < frameInfo->viewCount(); ++i) {
            const auto& view = frameInfo->view(i);
            const glm::mat4 eyePose = glm::inverse(Mat4FromVarjo(view.viewMatrix));
            positionSum += glm::vec3(eyePose[3]);
            if (!rotationSet) {
                rotation = glm::quat_cast(eyePose);
                rotationSet = true;
            }
            ++count;
        }

        if (count > 0) {
            const glm::vec3 position = positionSum / static_cast<float>(count);
            head = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation);
        }
        return head;
    }

    glm::mat4 worldMatrixForPlane(const XRPlane& plane, const glm::mat4& headMatrix) const {
        const glm::mat4 local = PlaneLocalMatrix(plane);
        if (plane.placementMode() == PlacementMode::HeadRelative) {
            return headMatrix * local;
        }
        return local;
    }

    void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) {
        if (!frameBegun || !acquired) {
            throw std::runtime_error("D3D11Backend::render requires beginFrame first.");
        }
        auto* ctx = core->GetImmediateContext();
        auto& image = swapImages[static_cast<size_t>(acquiredImageIndex)];
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const glm::mat4 headMatrix = computeHeadMatrix();

        for (int32_t viewIndex = 0; viewIndex < frameInfo->viewCount(); ++viewIndex) {
            auto* rtv = image.rtvs[static_cast<size_t>(viewIndex)].Get();
            ctx->ClearRenderTargetView(rtv, clear);
            ctx->OMSetRenderTargets(1, &rtv, nullptr);

            const auto& viewInfo = frameInfo->view(viewIndex);
            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(std::max(1, viewInfo.preferredWidth));
            viewport.Height = static_cast<float>(std::max(1, viewInfo.preferredHeight));
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &viewport);

            varjo_SwapChainViewport layerViewport = swapChain->fullViewport(viewIndex);
            layerViewport.width = std::max(1, viewInfo.preferredWidth);
            layerViewport.height = std::max(1, viewInfo.preferredHeight);
            multiProjLayer->setView(
                static_cast<size_t>(viewIndex),
                viewInfo.projectionMatrix,
                viewInfo.viewMatrix,
                layerViewport,
                nullptr);

            const Eye eye = EyeFromDescription(viewDescriptions[static_cast<size_t>(viewIndex)]);
            const glm::mat4 viewMatrix = Mat4FromVarjo(viewInfo.viewMatrix);
            const glm::mat4 projectionMatrix = Mat4FromVarjo(viewInfo.projectionMatrix);

            for (const auto& plane : planes) {
                if (!plane) continue;
                drawPlane(*plane, eye, viewMatrix, projectionMatrix, headMatrix, frameContext);
            }
        }

        ID3D11RenderTargetView* nullRtv = nullptr;
        ctx->OMSetRenderTargets(1, &nullRtv, nullptr);
        core->Flush();
    }

    void drawPlane(
        const XRPlane& plane,
        Eye eye,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const glm::mat4& headMatrix,
        const FrameContext& frameContext) {
        auto* ctx = core->GetImmediateContext();
        const XRMaterial& material = plane.material(eye);
        auto& pipeline = pipelineFor(material);
        pipeline.Bind(ctx);

        PlaneConstants constants{};
        CopyMatrix(constants.world, worldMatrixForPlane(plane, headMatrix));
        CopyMatrix(constants.view, viewMatrix);
        CopyMatrix(constants.projection, projectionMatrix);
        CopyVec4(constants.tint, material.tint);
        CopyVec4(constants.params0, material.params0);
        CopyVec4(constants.params1, material.params1);
        constants.frameParams[0] = frameContext.gazeUv.x;
        constants.frameParams[1] = frameContext.gazeUv.y;
        constants.frameParams[2] = static_cast<float>(frameContext.timeSeconds);
        constants.frameParams[3] = static_cast<float>(frameContext.frameNumber);

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
        if (!texture || !texture->srv()) {
            texture = whiteTexture;
        }
        ID3D11ShaderResourceView* srv = texture ? texture->srv() : nullptr;
        ID3D11SamplerState* samplerState = sampler.Get();
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->PSSetSamplers(0, 1, &samplerState);

        ctx->DrawIndexed(kPlaneIndexCount, 0, 0);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        ctx->PSSetShaderResources(0, 1, &nullSrv);
    }

    void endFrame() {
        if (!frameBegun) {
            throw std::runtime_error("D3D11Backend::endFrame requires beginFrame first.");
        }
        if (acquired) {
            swapChain->release();
            acquired = false;
            acquiredImageIndex = -1;
        }
        if (!layerFrame->end(*multiProjLayer, frameInfo->frameNumber())) {
            frameBegun = false;
            throw std::runtime_error("D3D11Backend::endFrame: VarjoLayerFrame::end failed: " + layerFrame->lastError());
        }
        frameBegun = false;
    }

    std::shared_ptr<D3D11Texture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) {
        if (!rgba || width == 0 || height == 0) {
            throw std::runtime_error("D3D11Backend::createTextureFromRGBA received invalid input.");
        }
        auto resource = D3D11CoreLib::CreateTexture2DFromMemory(
            *core,
            rgba,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            rowPitchBytes);
        auto srv = D3D11CoreLib::CreateTexture2DSrv(*core, resource);
        return std::make_shared<D3D11Texture>(std::move(resource), std::move(srv), width, height, TextureOwnership::Owned);
    }
};

D3D11Backend::D3D11Backend(std::shared_ptr<D3D11CoreLib::D3D11Core> core, D3D11BackendDesc desc)
    : impl_(std::make_unique<Impl>(std::move(core), desc)) {}

D3D11Backend::~D3D11Backend() = default;

void D3D11Backend::initialize(std::shared_ptr<::VarjoSession> session) { impl_->initialize(std::move(session)); }
void D3D11Backend::beginFrame() { impl_->beginFrame(); }
void D3D11Backend::render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) { impl_->render(planes, frameContext); }
void D3D11Backend::endFrame() { impl_->endFrame(); }

std::shared_ptr<D3D11Texture> D3D11Backend::createTextureFromRGBA(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes) {
    return impl_->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}

std::shared_ptr<D3D11Texture> D3D11Backend::wrapTexture(ID3D11Texture2D* texture, DXGI_FORMAT srvFormat) {
    if (!texture) {
        throw std::runtime_error("D3D11Backend::wrapTexture received null texture.");
    }
    D3D11CoreLib::ComPtr<ID3D11Texture2D> texturePtr = texture;
    D3D11CoreLib::D3D11Resource resource(texturePtr);
    auto srv = D3D11CoreLib::CreateTexture2DSrv(*impl_->core, resource, srvFormat);
    const auto desc = resource.GetTexture2DDesc();
    return std::make_shared<D3D11Texture>(std::move(resource), std::move(srv), desc.Width, desc.Height, TextureOwnership::External);
}

std::shared_ptr<D3D11Texture> D3D11Backend::wrapSrv(ID3D11ShaderResourceView* srv, uint32_t width, uint32_t height) {
    if (!srv) {
        throw std::runtime_error("D3D11Backend::wrapSrv received null SRV.");
    }
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srvPtr = srv;
    return std::make_shared<D3D11Texture>(std::move(srvPtr), width, height);
}

std::unique_ptr<IRenderBackend> CreateBackend(std::shared_ptr<D3D11CoreLib::D3D11Core> core, D3D11BackendDesc desc) {
    return std::make_unique<D3D11Backend>(std::move(core), desc);
}

} // namespace VarjoXR::Backends::D3D11
