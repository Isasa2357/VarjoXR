#include <VarjoXR/VarjoXR.hpp>

#if defined(VARJOXR_ENABLE_D3D11)
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp>
#endif

#if defined(VARJOXR_ENABLE_D3D12)
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Framework/D3D12GraphicsPipeline.hpp>
#include <D3D12Helper/D3D12Framework/D3D12Helpers.hpp>
#include <D3D12Helper/D3D12Framework/D3D12ShaderCompiler.hpp>
#endif

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* kVertexShader = R"hlsl(
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
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    return output;
}
)hlsl";

// Intentionally use the legacy one-parameter entry form. The helper compilers
// should wrap this into a struct input so TEXCOORD0 links correctly to the VS.
constexpr const char* kLegacyPixelShader = R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    return float4(uv.x, uv.y, 0.25f, 1.0f);
}
)hlsl";

struct Vertex {
    float position[3];
    float uv[2];
};

constexpr Vertex kQuadVertices[] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
};

constexpr std::uint16_t kQuadIndices[] = {0, 1, 2, 0, 2, 3};
constexpr UINT kWidth = 4;
constexpr UINT kHeight = 4;

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

UINT Align(UINT value, UINT alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

void CheckUvGradient(const std::uint8_t* pixels, UINT rowPitch, const char* label) {
    const std::uint8_t* topLeft = pixels;
    const std::uint8_t* bottomRight = pixels + rowPitch * (kHeight - 1u) + 4u * (kWidth - 1u);

    const int redDelta = static_cast<int>(bottomRight[0]) - static_cast<int>(topLeft[0]);
    const int greenDelta = static_cast<int>(bottomRight[1]) - static_cast<int>(topLeft[1]);

    if (redDelta <= 80 || greenDelta <= 80) {
        std::string message = std::string(label) + " UV propagation failed: top-left=(" +
            std::to_string(topLeft[0]) + "," + std::to_string(topLeft[1]) + ") bottom-right=(" +
            std::to_string(bottomRight[0]) + "," + std::to_string(bottomRight[1]) + ")";
        throw std::runtime_error(message);
    }
}

#if defined(VARJOXR_ENABLE_D3D11)
void RunD3D11ShaderLinkageTest() {
    std::cout << "[VarjoXRShaderLinkageTests] D3D11 offscreen UV linkage test...\n";

    auto core = D3D11CoreLib::D3D11Core::CreateShared();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* ctx = core->GetImmediateContext();
    Check(device != nullptr && ctx != nullptr, "D3D11 device/context is null.");

    auto vsBytecode = D3D11CoreLib::CompileShaderFromSource_D3DCompile(
        kVertexShader, "main", "vs_5_0", "VarjoXRShaderLinkage_D3D11_VS.hlsl");
    auto psBytecode = D3D11CoreLib::CompileShaderFromSource_D3DCompile(
        kLegacyPixelShader, "main", "ps_5_0", "VarjoXRShaderLinkage_D3D11_PS.hlsl");

    D3D11CoreLib::ComPtr<ID3D11VertexShader> vs;
    D3D11CoreLib::ComPtr<ID3D11PixelShader> ps;
    Check(SUCCEEDED(device->CreateVertexShader(vsBytecode.Data(), vsBytecode.Size(), nullptr, &vs)), "CreateVertexShader failed.");
    Check(SUCCEEDED(device->CreatePixelShader(psBytecode.Data(), psBytecode.Size(), nullptr, &ps)), "CreatePixelShader failed.");

    const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    D3D11CoreLib::ComPtr<ID3D11InputLayout> inputLayout;
    Check(SUCCEEDED(device->CreateInputLayout(
        inputLayoutDesc,
        static_cast<UINT>(std::size(inputLayoutDesc)),
        vsBytecode.Data(),
        vsBytecode.Size(),
        &inputLayout)), "CreateInputLayout failed.");

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(kQuadVertices));
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = kQuadVertices;
    D3D11CoreLib::ComPtr<ID3D11Buffer> vb;
    Check(SUCCEEDED(device->CreateBuffer(&vbDesc, &vbData, &vb)), "Create vertex buffer failed.");

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(kQuadIndices));
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = kQuadIndices;
    D3D11CoreLib::ComPtr<ID3D11Buffer> ib;
    Check(SUCCEEDED(device->CreateBuffer(&ibDesc, &ibData, &ib)), "Create index buffer failed.");

    D3D11_TEXTURE2D_DESC rtDesc{};
    rtDesc.Width = kWidth;
    rtDesc.Height = kHeight;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    D3D11CoreLib::ComPtr<ID3D11Texture2D> renderTarget;
    D3D11CoreLib::ComPtr<ID3D11RenderTargetView> rtv;
    Check(SUCCEEDED(device->CreateTexture2D(&rtDesc, nullptr, &renderTarget)), "Create render target failed.");
    Check(SUCCEEDED(device->CreateRenderTargetView(renderTarget.Get(), nullptr, &rtv)), "Create RTV failed.");

    D3D11_TEXTURE2D_DESC stagingDesc = rtDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    D3D11CoreLib::ComPtr<ID3D11Texture2D> staging;
    Check(SUCCEEDED(device->CreateTexture2D(&stagingDesc, nullptr, &staging)), "Create staging texture failed.");

    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx->ClearRenderTargetView(rtv.Get(), clear);
    ID3D11RenderTargetView* rtvPtr = rtv.Get();
    ctx->OMSetRenderTargets(1, &rtvPtr, nullptr);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(kWidth);
    viewport.Height = static_cast<float>(kHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer* vbPtr = vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vbPtr, &stride, &offset);
    ctx->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(inputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs.Get(), nullptr, 0);
    ctx->PSSetShader(ps.Get(), nullptr, 0);
    ctx->DrawIndexed(static_cast<UINT>(std::size(kQuadIndices)), 0, 0);

    ctx->CopyResource(staging.Get(), renderTarget.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Map staging texture failed.");
    CheckUvGradient(static_cast<const std::uint8_t*>(mapped.pData), mapped.RowPitch, "D3D11");
    ctx->Unmap(staging.Get(), 0);
}
#endif

#if defined(VARJOXR_ENABLE_D3D12)
D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 sizeBytes) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

void UploadToBuffer(ID3D12Resource* buffer, const void* data, std::size_t sizeBytes) {
    void* mapped = nullptr;
    D3D12_RANGE range{0, 0};
    Check(SUCCEEDED(buffer->Map(0, &range, &mapped)), "Map upload buffer failed.");
    std::memcpy(mapped, data, sizeBytes);
    buffer->Unmap(0, nullptr);
}

void RunD3D12ShaderLinkageTest() {
    std::cout << "[VarjoXRShaderLinkageTests] D3D12 offscreen UV linkage test...\n";

    D3D12CoreLib::D3D12CoreConfig config{};
    config.createDirectQueue = true;
    config.createCopyQueue = false;
    config.allowWarpAdapter = true;
    auto core = D3D12CoreLib::D3D12Core::CreateShared(config);
    ID3D12Device* device = core->GetDevice();
    Check(device != nullptr, "D3D12 device is null.");

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    D3D12CoreLib::ComPtr<ID3DBlob> rootBlob;
    D3D12CoreLib::ComPtr<ID3DBlob> rootError;
    Check(SUCCEEDED(D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &rootBlob,
        &rootError)), "D3D12SerializeRootSignature failed.");

    D3D12CoreLib::ComPtr<ID3D12RootSignature> rootSignature;
    Check(SUCCEEDED(device->CreateRootSignature(
        0,
        rootBlob->GetBufferPointer(),
        rootBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature))), "CreateRootSignature failed.");

    auto vsBytecode = D3D12CoreLib::CompileShaderFromSource_D3DCompile(
        kVertexShader, "main", "vs_5_1", "VarjoXRShaderLinkage_D3D12_VS.hlsl");
    auto psBytecode = D3D12CoreLib::CompileShaderFromSource_D3DCompile(
        kLegacyPixelShader, "main", "ps_5_1", "VarjoXRShaderLinkage_D3D12_PS.hlsl");

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    auto rasterizer = D3D12CoreLib::PipelineDefaults::Rasterizer(D3D12_CULL_MODE_NONE);
    auto blend = D3D12CoreLib::PipelineDefaults::BlendOpaque();
    auto depth = D3D12CoreLib::PipelineDefaults::DepthDisabled();

    D3D12CoreLib::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.vs = vsBytecode;
    pipelineDesc.ps = psBytecode;
    pipelineDesc.inputLayout = inputLayout;
    pipelineDesc.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.numRenderTargets = 1;
    pipelineDesc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.rasterizer = &rasterizer;
    pipelineDesc.blend = &blend;
    pipelineDesc.depthStencil = &depth;

    D3D12CoreLib::D3D12GraphicsPipeline pipeline;
    pipeline.Initialize(device, rootSignature, pipelineDesc);

    auto renderTarget = D3D12CoreLib::CreateTexture2D(
        *core,
        kWidth,
        kHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    D3D12CoreLib::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Check(SUCCEEDED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))), "Create RTV heap failed.");
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12CoreLib::CreateTexture2DRtv(*core, renderTarget, rtvHandle, DXGI_FORMAT_R8G8B8A8_UNORM);

    auto vertexBuffer = D3D12CoreLib::CreateBuffer(
        *core,
        sizeof(kQuadVertices),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    auto indexBuffer = D3D12CoreLib::CreateBuffer(
        *core,
        sizeof(kQuadIndices),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    UploadToBuffer(vertexBuffer.Get(), kQuadVertices, sizeof(kQuadVertices));
    UploadToBuffer(indexBuffer.Get(), kQuadIndices, sizeof(kQuadIndices));

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = vertexBuffer.Get()->GetGPUVirtualAddress();
    vbv.SizeInBytes = static_cast<UINT>(sizeof(kQuadVertices));
    vbv.StrideInBytes = sizeof(Vertex);

    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = indexBuffer.Get()->GetGPUVirtualAddress();
    ibv.SizeInBytes = static_cast<UINT>(sizeof(kQuadIndices));
    ibv.Format = DXGI_FORMAT_R16_UINT;

    const UINT readbackRowPitch = Align(kWidth * 4u, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const UINT64 readbackBytes = static_cast<UINT64>(readbackRowPitch) * kHeight;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12CoreLib::ComPtr<ID3D12Resource> readback;
    auto readbackDesc = MakeBufferDesc(readbackBytes);
    Check(SUCCEEDED(device->CreateCommittedResource(
        &readbackHeap,
        D3D12_HEAP_FLAG_NONE,
        &readbackDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readback))), "Create readback buffer failed.");

    auto ctx = core->CreateDirectContext();
    ctx.Reset();
    ID3D12GraphicsCommandList* cmd = ctx.GetCommandList();

    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(kWidth);
    viewport.Height = static_cast<float>(kHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor{0, 0, static_cast<LONG>(kWidth), static_cast<LONG>(kHeight)};
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);

    pipeline.Bind(cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->IASetIndexBuffer(&ibv);
    cmd->DrawIndexedInstanced(static_cast<UINT>(std::size(kQuadIndices)), 1, 0, 0, 0);

    D3D12_RESOURCE_BARRIER toCopy{};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = renderTarget.Get();
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    ctx.ResourceBarrier(toCopy);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Offset = 0;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = kWidth;
    dst.PlacedFootprint.Footprint.Height = kHeight;
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = readbackRowPitch;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = renderTarget.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    ctx.Close();
    ID3D12CommandList* lists[] = {cmd};
    core->DirectQueue().ExecuteCommandLists(1, lists);
    core->DirectQueue().WaitIdle();

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(readbackBytes)};
    Check(SUCCEEDED(readback->Map(0, &readRange, &mapped)), "Map readback buffer failed.");
    CheckUvGradient(static_cast<const std::uint8_t*>(mapped), readbackRowPitch, "D3D12");
    D3D12_RANGE writtenRange{0, 0};
    readback->Unmap(0, &writtenRange);
}
#endif

} // namespace

int main() {
    try {
#if defined(VARJOXR_ENABLE_D3D11)
        RunD3D11ShaderLinkageTest();
#endif
#if defined(VARJOXR_ENABLE_D3D12)
        RunD3D12ShaderLinkageTest();
#endif
        std::cout << "[VarjoXRShaderLinkageTests] All enabled backend tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[VarjoXRShaderLinkageTests] FAILED: " << e.what() << '\n';
        return 1;
    }
}
