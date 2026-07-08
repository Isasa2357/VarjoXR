#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>

#include <VarjoXR/Core/XRPlane.hpp>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d12.h>
#include <Varjo_layers.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <d3dcompiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <windows.h>

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
    float params0[4];
    float params1[4];
    float frameParams[4];
};

struct XRTextureProcessingFrameConstants {
    uint32_t srcWidth = 0;
    uint32_t srcHeight = 0;
    uint32_t dstWidth = 0;
    uint32_t dstHeight = 0;
    float frameParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

constexpr PlaneVertex kPlaneVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
};

constexpr uint16_t kPlaneIndices[] = {0, 1, 2, 0, 2, 3};
constexpr UINT kPlaneIndexCount = static_cast<UINT>(sizeof(kPlaneIndices) / sizeof(kPlaneIndices[0]));
constexpr UINT64 kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
constexpr UINT64 kMaxPlaneDrawsPerFrame = 1024;
constexpr UINT64 kPlaneConstantStride = (sizeof(PlaneConstants) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
constexpr UINT kTextureProcessingThreadGroupSizeX = 8;
constexpr UINT kTextureProcessingThreadGroupSizeY = 8;

constexpr UINT kProcessingSrvTableRootIndex = 0;
constexpr UINT kProcessingUavTableRootIndex = 1;
constexpr UINT kProcessingUserConstantsRootIndex = 2;
constexpr UINT kProcessingFrameConstantsRootIndex = 3;

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

DXGI_FORMAT ResolveProcessingOutputFormat(DXGI_FORMAT inputFormat) noexcept {
    switch (inputFormat) {
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_UNKNOWN:
    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

Eye EyeFromDescription(const varjo_ViewDescription& desc) noexcept {
    return desc.eye == varjo_Eye_Right ? Eye::Right : Eye::Left;
}

glm::mat4 Mat4FromArray(const double values[16]) noexcept {
    glm::mat4 out(1.0f);
    for (int i = 0; i < 16; ++i) {
        glm::value_ptr(out)[i] = static_cast<float>(values[i]);
    }
    return out;
}

varjo_Matrix VarjoMatrixFromArray(const double values[16]) noexcept {
    varjo_Matrix out{};
    for (int i = 0; i < 16; ++i) {
        out.value[i] = values[i];
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

bool FrameConstantsDescEquals(const TextureProcessingFrameConstantsDesc& a, const TextureProcessingFrameConstantsDesc& b) noexcept {
    return a.enabled == b.enabled && a.registerIndex == b.registerIndex;
}

bool UserConstantsLayoutEquals(const TextureProcessingConstantBuffer& a, const TextureProcessingConstantBuffer& b) noexcept {
    return a.registerIndex == b.registerIndex && a.data.size() == b.data.size();
}

bool ProcessingPipelineDescEquals(const TextureProcessingDesc& a, const TextureProcessingDesc& b) noexcept {
    return a.hlsl == b.hlsl &&
        a.entryPoint == b.entryPoint &&
        a.target == b.target &&
        a.sourceName == b.sourceName &&
        a.includeDirs == b.includeDirs &&
        a.outputSize == b.outputSize &&
        UserConstantsLayoutEquals(a.userConstants, b.userConstants) &&
        FrameConstantsDescEquals(a.frameConstants, b.frameConstants);
}

UINT CeilDiv(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
}

UINT64 AlignConstantBufferBytes(std::size_t sizeBytes) {
    if (sizeBytes == 0) {
        return 0;
    }
    return (static_cast<UINT64>(sizeBytes) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
}

void ValidateConstantRegisterIndex(uint32_t registerIndex, const char* label) {
    if (registerIndex >= D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
        throw std::runtime_error(std::string(label) + " register index exceeds D3D12 constant buffer slots.");
    }
}

D3D12_RESOURCE_BARRIER MakeTransition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after,
    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = subresource;
    return barrier;
}

} // namespace

D3D12Texture::D3D12Texture(
    D3D12CoreLib::D3D12Resource resource,
    D3D12CoreLib::D3D12DescriptorHandle srv,
    uint32_t width,
    uint32_t height,
    TextureOwnership ownership)
    : XRTexture(BackendType::D3D12, width, height, ownership)
    , resource_(std::move(resource))
    , srv_(srv) {}

struct D3D12Backend::Impl {
    std::shared_ptr<D3D12CoreLib::D3D12Core> core;
    D3D12BackendDesc desc{};
    D3D12CoreLib::D3D12DescriptorAllocator cbvSrvUavAllocator;
    D3D12CoreLib::D3D12DescriptorAllocator rtvAllocator;
    D3D12CoreLib::D3D12CommandContext commandContext;
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<::VarjoFrameInfo> frameInfo;
    std::unique_ptr<::VarjoLayerFrame> layerFrame;
    std::unique_ptr<::VarjoMultiProjLayer> multiProjLayer;
    std::unique_ptr<::VarjoSwapChain> swapChain;
    bool frameBegun = false;
    bool commandListClosed = false;
    bool acquired = false;
    int32_t acquiredImageIndex = -1;
    int32_t viewCount = 0;
    int32_t swapchainWidth = 1;
    int32_t swapchainHeight = 1;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    std::vector<varjo_ViewDescription> viewDescriptions;

    D3D12CoreLib::D3D12UploadBuffer vertexUpload;
    D3D12CoreLib::D3D12UploadBuffer indexUpload;
    D3D12CoreLib::D3D12UploadBuffer constantUpload;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    UINT64 constantOffset = 0;
    std::shared_ptr<D3D12Texture> whiteTexture;

    struct ProcessingCacheEntry {
        std::weak_ptr<D3D12Texture> lastDispatchedSource;
        TextureProcessingDesc pipelineDesc{};
        D3D12CoreLib::ComPtr<ID3D12RootSignature> rootSignature;
        D3D12CoreLib::ComPtr<ID3D12PipelineState> pipelineState;
        std::shared_ptr<D3D12Texture> finalTexture;
        D3D12CoreLib::D3D12Resource output;
        D3D12CoreLib::D3D12DescriptorHandle outputSrv{};
        D3D12CoreLib::D3D12DescriptorHandle outputUav{};
        D3D12CoreLib::D3D12UploadBuffer userConstantUpload;
        D3D12CoreLib::D3D12UploadBuffer frameConstantUpload;
        std::vector<std::byte> alignedUserConstants;
        bool hasDispatched = false;
    };
    std::unordered_map<const XRMaterial*, ProcessingCacheEntry> processingCache;

    D3D12CoreLib::ComPtr<ID3D12RootSignature> rootSignature;
    D3D12CoreLib::ShaderBytecode vertexShaderBytecode;
    std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline> defaultPipeline;
    std::unordered_map<std::string, std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline>> pipelineCache;

    struct SwapImageViews {
        D3D12CoreLib::D3D12Resource resource;
        std::vector<D3D12CoreLib::D3D12DescriptorHandle> rtvs;
    };
    std::vector<SwapImageViews> swapImages;

    explicit Impl(std::shared_ptr<D3D12CoreLib::D3D12Core> inCore, D3D12BackendDesc inDesc)
        : core(std::move(inCore)), desc(inDesc) {
        if (!core) {
            throw std::runtime_error("D3D12Backend requires an externally-created D3D12Core shared_ptr.");
        }
        cbvSrvUavAllocator.Initialize(
            core->GetDevice(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            desc.cbvSrvUavDescriptorCount,
            true);
        rtvAllocator.Initialize(
            core->GetDevice(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            desc.rtvDescriptorCount,
            false);
        commandContext = core->CreateDirectContext();
    }

    void initialize(std::shared_ptr<::VarjoSession> inSession) {
        if (!inSession || !inSession->valid()) {
            throw std::runtime_error("D3D12Backend::initialize requires a valid external VarjoSession.");
        }
        session = std::move(inSession);
        frameInfo = std::make_unique<::VarjoFrameInfo>(*session);
        layerFrame = std::make_unique<::VarjoLayerFrame>(*session);
        if (!frameInfo || !frameInfo->valid()) {
            throw std::runtime_error("D3D12Backend failed to create VarjoFrameInfo.");
        }
        if (!layerFrame || !layerFrame->valid()) {
            throw std::runtime_error("D3D12Backend failed to create VarjoLayerFrame.");
        }

        createSwapChain();
        createPlaneResources();
        createRootSignature();
        createWhiteTexture();
        createDefaultPipeline();

        OutputDebugStringA("[VarjoXR][D3D12] Backend initialized with external D3D12Core and external VarjoSession.\n");
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

        auto chain = VarjoSwapChain::createD3D12(session->shared(), core->GetDirectCommandQueue(), config);
        if (!chain.valid()) {
            throw std::runtime_error("D3D12Backend failed to create VarjoSwapChain: " + chain.lastError());
        }
        swapChain = std::make_unique<VarjoSwapChain>(std::move(chain));
        multiProjLayer = std::make_unique<VarjoMultiProjLayer>(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);

        createSwapImageViews();
    }

    void createSwapImageViews() {
        swapImages.resize(static_cast<size_t>(swapChain->config().numberOfTextures));
        for (int32_t imageIndex = 0; imageIndex < swapChain->config().numberOfTextures; ++imageIndex) {
            const varjo_Texture varjoTexture = swapChain->image(imageIndex);
            ID3D12Resource* rawResource = varjo_ToD3D12Texture(varjoTexture);
            if (!rawResource) {
                throw std::runtime_error("D3D12Backend: varjo_ToD3D12Texture returned null.");
            }

            D3D12CoreLib::ComPtr<ID3D12Resource> resourcePtr = rawResource;
            auto& image = swapImages[static_cast<size_t>(imageIndex)];
            image.resource = D3D12CoreLib::D3D12Resource(resourcePtr, D3D12_RESOURCE_STATE_COMMON);
            image.rtvs.resize(static_cast<size_t>(viewCount));

            for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = rtvFormat;
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(viewIndex);
                rtvDesc.Texture2DArray.ArraySize = 1;
                auto rtv = rtvAllocator.Allocate();
                D3D12CoreLib::CreateRtv(*core, rawResource, rtvDesc, rtv.cpu);
                image.rtvs[static_cast<size_t>(viewIndex)] = rtv;
            }
        }
    }

    void createPlaneResources() {
        vertexUpload.Initialize(core->GetDevice(), sizeof(kPlaneVertices));
        std::memcpy(vertexUpload.Map(), kPlaneVertices, sizeof(kPlaneVertices));
        vertexBufferView.BufferLocation = vertexUpload.Get()->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(kPlaneVertices));
        vertexBufferView.StrideInBytes = sizeof(PlaneVertex);

        indexUpload.Initialize(core->GetDevice(), sizeof(kPlaneIndices));
        std::memcpy(indexUpload.Map(), kPlaneIndices, sizeof(kPlaneIndices));
        indexBufferView.BufferLocation = indexUpload.Get()->GetGPUVirtualAddress();
        indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(kPlaneIndices));
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;

        constantUpload.Initialize(core->GetDevice(), kPlaneConstantStride * kMaxPlaneDrawsPerFrame);
    }

    void createRootSignature() {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].Descriptor.RegisterSpace = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        parameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MaxLOD = FLT_MAX;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = static_cast<UINT>(std::size(parameters));
        rootDesc.pParameters = parameters;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12CoreLib::ComPtr<ID3DBlob> signature;
        D3D12CoreLib::ComPtr<ID3DBlob> error;
        const HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
                OutputDebugStringA("\n");
            }
            D3D12CoreLib::ThrowIfFailed(hr, "D3D12SerializeRootSignature failed");
        }
        D3D12CORE_THROW_IF_FAILED(core->GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(rootSignature.GetAddressOf())));
    }

    D3D12CoreLib::ComPtr<ID3D12RootSignature> createTextureProcessingRootSignature(const TextureProcessingDesc& processing) {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 1;
        uavRange.BaseShaderRegister = 0;
        uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER parameters[4]{};
        parameters[kProcessingSrvTableRootIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[kProcessingSrvTableRootIndex].DescriptorTable.NumDescriptorRanges = 1;
        parameters[kProcessingSrvTableRootIndex].DescriptorTable.pDescriptorRanges = &srvRange;
        parameters[kProcessingSrvTableRootIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[kProcessingUavTableRootIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[kProcessingUavTableRootIndex].DescriptorTable.NumDescriptorRanges = 1;
        parameters[kProcessingUavTableRootIndex].DescriptorTable.pDescriptorRanges = &uavRange;
        parameters[kProcessingUavTableRootIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[kProcessingUserConstantsRootIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[kProcessingUserConstantsRootIndex].Descriptor.ShaderRegister = processing.userConstants.registerIndex;
        parameters[kProcessingUserConstantsRootIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[kProcessingFrameConstantsRootIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[kProcessingFrameConstantsRootIndex].Descriptor.ShaderRegister = processing.frameConstants.registerIndex;
        parameters[kProcessingFrameConstantsRootIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = static_cast<UINT>(std::size(parameters));
        rootDesc.pParameters = parameters;

        D3D12CoreLib::ComPtr<ID3DBlob> signature;
        D3D12CoreLib::ComPtr<ID3DBlob> error;
        const HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
                OutputDebugStringA("\n");
            }
            D3D12CoreLib::ThrowIfFailed(hr, "D3D12SerializeRootSignature for texture processing failed");
        }

        D3D12CoreLib::ComPtr<ID3D12RootSignature> result;
        D3D12CORE_THROW_IF_FAILED(core->GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(result.GetAddressOf())));
        return result;
    }

    void createWhiteTexture() {
        const uint8_t white[4] = {255, 255, 255, 255};
        whiteTexture = createTextureFromRGBA(white, 1, 1, 4);
    }

    void createDefaultPipeline() {
        defaultPipeline = createPipeline(kDefaultPlanePixelShader, "VarjoXR_D3D12_DefaultPlanePS.hlsl");
    }

    std::unique_ptr<D3D12CoreLib::D3D12GraphicsPipeline> createPipeline(
        const std::string& pixelShaderSource,
        const std::string& sourceName) {
        if (vertexShaderBytecode.Empty()) {
            vertexShaderBytecode = D3D12CoreLib::CompileShaderFromSource_D3DCompile(
                kPlaneVertexShader,
                "main",
                "vs_5_1",
                "VarjoXR_D3D12_PlaneVS.hlsl");
        }

        auto pixelBytecode = D3D12CoreLib::CompileShaderFromSource_D3DCompile(
            pixelShaderSource,
            "main",
            "ps_5_1",
            sourceName);

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        auto rasterizer = D3D12CoreLib::PipelineDefaults::Rasterizer(D3D12_CULL_MODE_NONE);
        auto blend = desc.enableAlphaBlend ? D3D12CoreLib::PipelineDefaults::BlendAlpha()
                                           : D3D12CoreLib::PipelineDefaults::BlendOpaque();
        auto depth = D3D12CoreLib::PipelineDefaults::DepthDisabled();

        D3D12CoreLib::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.vs = vertexShaderBytecode;
        pipelineDesc.ps = pixelBytecode;
        pipelineDesc.inputLayout = inputLayout;
        pipelineDesc.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineDesc.numRenderTargets = 1;
        pipelineDesc.rtvFormats[0] = rtvFormat;
        pipelineDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
        pipelineDesc.rasterizer = &rasterizer;
        pipelineDesc.blend = &blend;
        pipelineDesc.depthStencil = &depth;

        auto pipeline = std::make_unique<D3D12CoreLib::D3D12GraphicsPipeline>();
        pipeline->Initialize(core->GetDevice(), rootSignature, pipelineDesc);
        return pipeline;
    }

    D3D12CoreLib::D3D12GraphicsPipeline& pipelineFor(const XRMaterial& material) {
        if (material.planePixelShaderHlsl.empty()) {
            return *defaultPipeline;
        }
        auto it = pipelineCache.find(material.planePixelShaderHlsl);
        if (it != pipelineCache.end()) {
            return *it->second;
        }
        try {
            auto pipeline = createPipeline(BuildUserPixelShader(material.planePixelShaderHlsl), "VarjoXR_D3D12_UserPlanePS.hlsl");
            auto* raw = pipeline.get();
            pipelineCache.emplace(material.planePixelShaderHlsl, std::move(pipeline));
            return *raw;
        } catch (const std::exception& e) {
            OutputDebugStringA("[VarjoXR][D3D12] Plane HLSL compile failed. Falling back to default shader.\n");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
            return *defaultPipeline;
        }
    }

    D3D12CoreLib::ShaderBytecode compileTextureProcessingShader(const TextureProcessingDesc& processing) {
        D3D12CoreLib::ShaderCompileDesc compileDesc{};
        compileDesc.entryPoint = processing.entryPoint.empty() ? "main" : processing.entryPoint;
        compileDesc.target = processing.target.empty() ? "cs_5_1" : processing.target;
        compileDesc.includeDirs = processing.includeDirs;
        compileDesc.useDxc = false;
        return D3D12CoreLib::CompileShaderFromSource(
            processing.hlsl,
            compileDesc,
            processing.sourceName.empty() ? "VarjoXR_TextureProcessing.hlsl" : processing.sourceName);
    }

    void createTextureProcessingPipeline(ProcessingCacheEntry& cache, const TextureProcessingDesc& processing) {
        auto bytecode = compileTextureProcessingShader(processing);
        cache.rootSignature = createTextureProcessingRootSignature(processing);
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = cache.rootSignature.Get();
        pipelineDesc.CS = bytecode.AsD3D12();
        D3D12CORE_THROW_IF_FAILED(core->GetDevice()->CreateComputePipelineState(
            &pipelineDesc,
            IID_PPV_ARGS(cache.pipelineState.GetAddressOf())));
    }

    bool ensureTextureProcessingCache(
        const XRMaterial& material,
        const std::shared_ptr<D3D12Texture>& sourceTexture,
        ProcessingCacheEntry& cache) {
        const auto& processing = material.processing;
        const UINT dstWidth = processing.outputSize.x > 0 ? processing.outputSize.x : sourceTexture->width();
        const UINT dstHeight = processing.outputSize.y > 0 ? processing.outputSize.y : sourceTexture->height();
        const bool pipelineMatches = ProcessingPipelineDescEquals(cache.pipelineDesc, processing);
        const bool outputMatches = cache.finalTexture && cache.finalTexture->width() == dstWidth && cache.finalTexture->height() == dstHeight;

        if (pipelineMatches && outputMatches && cache.pipelineState && cache.rootSignature && cache.outputUav.IsValid()) {
            return true;
        }

        if (processing.hlsl.empty()) {
            OutputDebugStringA("[VarjoXR][D3D12] Texture processing is enabled but no HLSL was supplied. Using source texture.\n");
            return false;
        }
        if (!processing.userConstants.data.empty() &&
            processing.frameConstants.enabled &&
            processing.userConstants.registerIndex == processing.frameConstants.registerIndex) {
            throw std::runtime_error("Texture processing user constant buffer register collides with VarjoXR frame constants register.");
        }
        if (!processing.userConstants.data.empty()) {
            ValidateConstantRegisterIndex(processing.userConstants.registerIndex, "Texture processing user constants");
        }
        if (processing.frameConstants.enabled) {
            ValidateConstantRegisterIndex(processing.frameConstants.registerIndex, "Texture processing frame constants");
        }

        cache = ProcessingCacheEntry{};
        cache.pipelineDesc = processing;
        createTextureProcessingPipeline(cache, processing);

        const DXGI_FORMAT outputFormat = ResolveProcessingOutputFormat(sourceTexture->resource().GetFormat());
        cache.output = D3D12CoreLib::CreateTexture2D(
            *core,
            dstWidth,
            dstHeight,
            outputFormat,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        cache.outputSrv = cbvSrvUavAllocator.Allocate();
        cache.outputUav = cbvSrvUavAllocator.Allocate();
        D3D12CoreLib::CreateTexture2DSrv(*core, cache.output, cache.outputSrv.cpu, outputFormat);
        D3D12CoreLib::CreateTexture2DUav(*core, cache.output, cache.outputUav.cpu, outputFormat);
        cache.finalTexture = std::make_shared<D3D12Texture>(cache.output, cache.outputSrv, dstWidth, dstHeight, TextureOwnership::Owned);

        const UINT64 userConstantBytes = AlignConstantBufferBytes(processing.userConstants.data.size());
        if (userConstantBytes > 0) {
            cache.alignedUserConstants.assign(static_cast<std::size_t>(userConstantBytes), std::byte{0});
            cache.userConstantUpload.Initialize(core->GetDevice(), userConstantBytes);
        }
        if (processing.frameConstants.enabled) {
            cache.frameConstantUpload.Initialize(core->GetDevice(), kConstantBufferAlignment);
        }
        cache.hasDispatched = false;
        return true;
    }

    void updateUserConstants(const XRMaterial& material, ProcessingCacheEntry& cache) {
        const auto& data = material.processing.userConstants.data;
        if (cache.alignedUserConstants.empty() || data.empty()) {
            return;
        }
        std::fill(cache.alignedUserConstants.begin(), cache.alignedUserConstants.end(), std::byte{0});
        std::memcpy(cache.alignedUserConstants.data(), data.data(), data.size());
        std::memcpy(cache.userConstantUpload.Map(), cache.alignedUserConstants.data(), cache.alignedUserConstants.size());
    }

    std::shared_ptr<D3D12Texture> resolveMaterialTexture(const XRMaterial& material, const FrameContext& frameContext) {
        auto texture = std::dynamic_pointer_cast<D3D12Texture>(material.texture);
        if (!texture || !texture->srv().IsValid()) {
            return whiteTexture;
        }
        if (!material.processing.enabled) {
            return texture;
        }
        if (!texture->resource()) {
            OutputDebugStringA("[VarjoXR][D3D12] Programmable texture processing requires an ID3D12Resource-backed texture. Passing through.\n");
            return texture;
        }

        auto& cache = processingCache[&material];
        if (!ensureTextureProcessingCache(material, texture, cache)) {
            return texture;
        }

        const auto lastSource = cache.lastDispatchedSource.lock();
        const bool skipDispatch = material.processing.timing == ProcessingTiming::OnTextureChanged &&
            cache.hasDispatched &&
            lastSource.get() == texture.get();
        if (!skipDispatch) {
            runTextureProcessing(material, texture, cache, frameContext);
        }
        return cache.finalTexture ? cache.finalTexture : texture;
    }

    void runTextureProcessing(
        const XRMaterial& material,
        const std::shared_ptr<D3D12Texture>& sourceTexture,
        ProcessingCacheEntry& cache,
        const FrameContext& frameContext) {
        if (!cache.rootSignature || !cache.pipelineState || !cache.finalTexture || !cache.outputUav.IsValid()) {
            return;
        }

        auto* cmd = commandContext.GetCommandList();
        auto& srcResource = sourceTexture->resource();
        const D3D12_RESOURCE_STATES srcBefore = srcResource.GetState();
        if (srcBefore != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) {
            auto barrier = MakeTransition(srcResource.Get(), srcBefore, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            commandContext.ResourceBarrier(barrier);
            srcResource.SetState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        if (cache.output.GetState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            auto barrier = MakeTransition(cache.output.Get(), cache.output.GetState(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            commandContext.ResourceBarrier(barrier);
            cache.output.SetState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        updateUserConstants(material, cache);
        if (material.processing.frameConstants.enabled) {
            XRTextureProcessingFrameConstants constants{};
            constants.srcWidth = sourceTexture->width();
            constants.srcHeight = sourceTexture->height();
            constants.dstWidth = cache.finalTexture->width();
            constants.dstHeight = cache.finalTexture->height();
            constants.frameParams[0] = frameContext.gazeUv.x;
            constants.frameParams[1] = frameContext.gazeUv.y;
            constants.frameParams[2] = static_cast<float>(frameContext.timeSeconds);
            constants.frameParams[3] = static_cast<float>(frameContext.frameNumber);
            std::memcpy(cache.frameConstantUpload.Map(), &constants, sizeof(constants));
        }

        cmd->SetPipelineState(cache.pipelineState.Get());
        cmd->SetComputeRootSignature(cache.rootSignature.Get());
        cmd->SetComputeRootDescriptorTable(kProcessingSrvTableRootIndex, sourceTexture->srv().gpu);
        cmd->SetComputeRootDescriptorTable(kProcessingUavTableRootIndex, cache.outputUav.gpu);
        if (!cache.alignedUserConstants.empty()) {
            cmd->SetComputeRootConstantBufferView(
                kProcessingUserConstantsRootIndex,
                cache.userConstantUpload.Get()->GetGPUVirtualAddress());
        }
        if (material.processing.frameConstants.enabled) {
            cmd->SetComputeRootConstantBufferView(
                kProcessingFrameConstantsRootIndex,
                cache.frameConstantUpload.Get()->GetGPUVirtualAddress());
        }

        const UINT groupsX = CeilDiv(cache.finalTexture->width(), kTextureProcessingThreadGroupSizeX);
        const UINT groupsY = CeilDiv(cache.finalTexture->height(), kTextureProcessingThreadGroupSizeY);
        cmd->Dispatch(groupsX, groupsY, 1);

        auto outputToSrv = MakeTransition(cache.output.Get(), cache.output.GetState(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandContext.ResourceBarrier(outputToSrv);
        cache.output.SetState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (srcBefore != srcResource.GetState()) {
            auto restore = MakeTransition(srcResource.Get(), srcResource.GetState(), srcBefore);
            commandContext.ResourceBarrier(restore);
            srcResource.SetState(srcBefore);
        }
        cache.lastDispatchedSource = sourceTexture;
        cache.hasDispatched = true;
    }

    void beginFrame() {
        if (frameBegun) {
            throw std::runtime_error("D3D12Backend::beginFrame called while a frame is already active.");
        }
        if (!frameInfo->waitSync()) {
            throw std::runtime_error("D3D12Backend::beginFrame: VarjoFrameInfo::waitSync failed: " + session->lastError());
        }
        if (!layerFrame->begin()) {
            throw std::runtime_error("D3D12Backend::beginFrame: VarjoLayerFrame::begin failed: " + layerFrame->lastError());
        }
        if (!swapChain->acquire(acquiredImageIndex)) {
            throw std::runtime_error("D3D12Backend::beginFrame: VarjoSwapChain::acquire failed: " + swapChain->lastError());
        }
        acquired = true;
        constantOffset = 0;
        commandContext.Reset();
        commandListClosed = false;
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
            const glm::mat4 eyePose = glm::inverse(Mat4FromArray(view.viewMatrix));
            positionSum += glm::vec3(eyePose[3]);
            if (!rotationSet) {
                rotation = glm::quat_cast(eyePose);
                rotationSet = true;
            }
            ++count;
        }

        if (count > 0) {
            const glm::vec3 position = positionSum / static_cast<float>(count);
            head = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
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
            throw std::runtime_error("D3D12Backend::render requires beginFrame first.");
        }

        auto* cmd = commandContext.GetCommandList();
        auto& image = swapImages[static_cast<size_t>(acquiredImageIndex)];
        const glm::mat4 headMatrix = computeHeadMatrix();

        D3D12_RESOURCE_BARRIER toRtv = MakeTransition(
            image.resource.Get(),
            image.resource.GetState(),
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandContext.ResourceBarrier(toRtv);
        image.resource.SetState(D3D12_RESOURCE_STATE_RENDER_TARGET);

        ID3D12DescriptorHeap* heaps[] = {cbvSrvUavAllocator.GetHeap()};
        cmd->SetDescriptorHeaps(1, heaps);

        for (int32_t viewIndex = 0; viewIndex < frameInfo->viewCount(); ++viewIndex) {
            const auto& viewInfo = frameInfo->view(viewIndex);
            auto rtv = image.rtvs[static_cast<size_t>(viewIndex)];
            const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            cmd->ClearRenderTargetView(rtv.cpu, clear, 0, nullptr);
            cmd->OMSetRenderTargets(1, &rtv.cpu, FALSE, nullptr);

            D3D12_VIEWPORT viewport{};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(std::max(1, viewInfo.preferredWidth));
            viewport.Height = static_cast<float>(std::max(1, viewInfo.preferredHeight));
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            cmd->RSSetViewports(1, &viewport);

            D3D12_RECT scissor{};
            scissor.left = 0;
            scissor.top = 0;
            scissor.right = std::max<LONG>(1, static_cast<LONG>(viewInfo.preferredWidth));
            scissor.bottom = std::max<LONG>(1, static_cast<LONG>(viewInfo.preferredHeight));
            cmd->RSSetScissorRects(1, &scissor);

            varjo_SwapChainViewport layerViewport = swapChain->fullViewport(viewIndex);
            layerViewport.width = std::max(1, viewInfo.preferredWidth);
            layerViewport.height = std::max(1, viewInfo.preferredHeight);
            const varjo_Matrix projection = VarjoMatrixFromArray(viewInfo.projectionMatrix);
            const varjo_Matrix view = VarjoMatrixFromArray(viewInfo.viewMatrix);
            multiProjLayer->setView(
                static_cast<size_t>(viewIndex),
                projection,
                view,
                layerViewport,
                nullptr);

            const Eye eye = EyeFromDescription(viewDescriptions[static_cast<size_t>(viewIndex)]);
            const glm::mat4 viewMatrix = Mat4FromArray(viewInfo.viewMatrix);
            const glm::mat4 projectionMatrix = Mat4FromArray(viewInfo.projectionMatrix);

            for (const auto& plane : planes) {
                if (!plane) continue;
                drawPlane(*plane, eye, viewMatrix, projectionMatrix, headMatrix, frameContext);
            }
        }

        D3D12_RESOURCE_BARRIER toCommon = MakeTransition(
            image.resource.Get(),
            image.resource.GetState(),
            D3D12_RESOURCE_STATE_COMMON);
        commandContext.ResourceBarrier(toCommon);
        image.resource.SetState(D3D12_RESOURCE_STATE_COMMON);

        commandContext.Close();
        commandListClosed = true;
    }

    void drawPlane(
        const XRPlane& plane,
        Eye eye,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const glm::mat4& headMatrix,
        const FrameContext& frameContext) {
        auto* cmd = commandContext.GetCommandList();
        const XRMaterial& material = plane.material(eye);
        auto texture = resolveMaterialTexture(material, frameContext);
        auto& pipeline = pipelineFor(material);
        pipeline.Bind(cmd);

        if (constantOffset + kPlaneConstantStride > constantUpload.GetSizeBytes()) {
            throw std::runtime_error("D3D12Backend constant buffer ring is too small for the number of plane draws in this frame.");
        }

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

        auto* dst = static_cast<uint8_t*>(constantUpload.Map()) + constantOffset;
        std::memcpy(dst, &constants, sizeof(constants));
        const D3D12_GPU_VIRTUAL_ADDRESS cbv = constantUpload.Get()->GetGPUVirtualAddress() + constantOffset;
        constantOffset += kPlaneConstantStride;

        if (!texture || !texture->srv().IsValid()) {
            texture = whiteTexture;
        }

        cmd->SetGraphicsRootConstantBufferView(0, cbv);
        if (texture && texture->srv().IsValid()) {
            cmd->SetGraphicsRootDescriptorTable(1, texture->srv().gpu);
        }

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmd->IASetIndexBuffer(&indexBufferView);
        cmd->DrawIndexedInstanced(kPlaneIndexCount, 1, 0, 0, 0);
    }

    void endFrame() {
        if (!frameBegun) {
            throw std::runtime_error("D3D12Backend::endFrame requires beginFrame first.");
        }

        if (!commandListClosed) {
            commandContext.Close();
            commandListClosed = true;
        }

        ID3D12CommandList* lists[] = {commandContext.GetCommandList()};
        core->DirectQueue().ExecuteCommandLists(1, lists);
        core->DirectQueue().WaitIdle();

        if (acquired) {
            swapChain->release();
            acquired = false;
            acquiredImageIndex = -1;
        }
        if (!layerFrame->end(*multiProjLayer, frameInfo->frameNumber())) {
            frameBegun = false;
            throw std::runtime_error("D3D12Backend::endFrame: VarjoLayerFrame::end failed: " + layerFrame->lastError());
        }
        frameBegun = false;
    }

    std::shared_ptr<D3D12Texture> createTextureFromRGBA(
        const uint8_t* rgba,
        uint32_t width,
        uint32_t height,
        uint32_t rowPitchBytes) {
        if (!rgba || width == 0 || height == 0) {
            throw std::runtime_error("D3D12Backend::createTextureFromRGBA received invalid input.");
        }
        auto resource = D3D12CoreLib::CreateTexture2DFromMemory(
            *core,
            rgba,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            rowPitchBytes,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        auto srv = cbvSrvUavAllocator.Allocate();
        D3D12CoreLib::CreateTexture2DSrv(*core, resource, srv.cpu);
        return std::make_shared<D3D12Texture>(std::move(resource), srv, width, height, TextureOwnership::Owned);
    }
};

D3D12Backend::D3D12Backend(std::shared_ptr<D3D12CoreLib::D3D12Core> core, D3D12BackendDesc desc)
    : impl_(std::make_unique<Impl>(std::move(core), desc)) {}

D3D12Backend::~D3D12Backend() = default;

void D3D12Backend::initialize(std::shared_ptr<::VarjoSession> session) { impl_->initialize(std::move(session)); }
void D3D12Backend::beginFrame() { impl_->beginFrame(); }
void D3D12Backend::render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) { impl_->render(planes, frameContext); }
void D3D12Backend::endFrame() { impl_->endFrame(); }

std::shared_ptr<D3D12Texture> D3D12Backend::createTextureFromRGBA(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes) {
    return impl_->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}

std::shared_ptr<D3D12Texture> D3D12Backend::wrapResource(ID3D12Resource* resource, DXGI_FORMAT srvFormat) {
    if (!resource) {
        throw std::runtime_error("D3D12Backend::wrapResource received null resource.");
    }
    D3D12CoreLib::ComPtr<ID3D12Resource> resourcePtr = resource;
    D3D12CoreLib::D3D12Resource wrapped(resourcePtr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const auto desc = wrapped.GetDesc();
    auto srv = impl_->cbvSrvUavAllocator.Allocate();
    D3D12CoreLib::CreateTexture2DSrv(*impl_->core, wrapped, srv.cpu, srvFormat);
    return std::make_shared<D3D12Texture>(
        std::move(wrapped),
        srv,
        static_cast<uint32_t>(desc.Width),
        static_cast<uint32_t>(desc.Height),
        TextureOwnership::External);
}

std::unique_ptr<IRenderBackend> CreateBackend(std::shared_ptr<D3D12CoreLib::D3D12Core> core, D3D12BackendDesc desc) {
    return std::make_unique<D3D12Backend>(std::move(core), desc);
}

} // namespace VarjoXR::Backends::D3D12
