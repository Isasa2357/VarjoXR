#pragma once

#include <VarjoXR/VarjoXR.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#if defined(RENDERING_PLANE_SAMPLE_D3D11)
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#elif defined(RENDERING_PLANE_SAMPLE_D3D12)
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#else
#error "Define RENDERING_PLANE_SAMPLE_D3D11 or RENDERING_PLANE_SAMPLE_D3D12."
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace RenderingPlaneSample {

constexpr std::uint32_t kDefaultTextureWidth = 512;
constexpr std::uint32_t kDefaultTextureHeight = 512;

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct CircleProcessingConstants {
    float centerX = 0.5f;
    float centerY = 0.5f;
    float radius = 0.28f;
    float outsideBrightness = 0.45f;

    float edgeSoftness = 0.035f;
    float pulseStrength = 0.035f;
    float reserved0 = 0.0f;
    float reserved1 = 0.0f;
};

inline const char* BackendName() noexcept {
#if defined(RENDERING_PLANE_SAMPLE_D3D11)
    return "D3D11";
#else
    return "D3D12";
#endif
}

inline std::shared_ptr<VarjoSession> CreateSessionOrThrow() {
    auto session = std::make_shared<VarjoSession>();
    if (!session->valid() && !session->initialize()) {
        throw std::runtime_error("Failed to initialize Varjo session: " + session->lastError());
    }
    return session;
}

inline std::unique_ptr<VarjoXR::IRenderBackend> CreateBackend() {
#if defined(RENDERING_PLANE_SAMPLE_D3D11)
    auto d3d = D3D11CoreLib::D3D11Core::CreateShared();
    return VarjoXR::Backends::D3D11::CreateBackend(d3d);
#else
    D3D12CoreLib::D3D12CoreConfig config{};
    config.createDirectQueue = true;
    config.createCopyQueue = true;
    auto d3d = D3D12CoreLib::D3D12Core::CreateShared(config);
    return VarjoXR::Backends::D3D12::CreateBackend(d3d);
#endif
}

inline std::vector<std::uint8_t> MakeSolidRgba(std::uint32_t width, std::uint32_t height, Rgba color) {
    std::vector<std::uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            rgba[i + 0] = color.r;
            rgba[i + 1] = color.g;
            rgba[i + 2] = color.b;
            rgba[i + 3] = color.a;
        }
    }
    return rgba;
}

inline std::vector<std::uint8_t> MakeCheckerRgba(
    std::uint32_t width,
    std::uint32_t height,
    Rgba a,
    Rgba b,
    std::uint32_t blockSize = 32) {
    std::vector<std::uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    blockSize = std::max<std::uint32_t>(1u, blockSize);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const bool checker = ((x / blockSize) + (y / blockSize)) % 2u == 0u;
            const Rgba c = checker ? a : b;
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            rgba[i + 0] = c.r;
            rgba[i + 1] = c.g;
            rgba[i + 2] = c.b;
            rgba[i + 3] = c.a;
        }
    }
    return rgba;
}

inline std::vector<std::uint8_t> MakeGradientRgba(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const float u = width > 1 ? static_cast<float>(x) / static_cast<float>(width - 1u) : 0.0f;
            const float v = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1u) : 0.0f;
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            rgba[i + 0] = static_cast<std::uint8_t>(255.0f * u);
            rgba[i + 1] = static_cast<std::uint8_t>(255.0f * v);
            rgba[i + 2] = static_cast<std::uint8_t>(255.0f * (1.0f - 0.5f * u));
            rgba[i + 3] = 255;
        }
    }
    return rgba;
}

inline std::shared_ptr<VarjoXR::XRTexture> CreateTextureFromRGBA(
    VarjoXR::XRSpace& space,
    const std::vector<std::uint8_t>& rgba,
    std::uint32_t width,
    std::uint32_t height) {
    if (rgba.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4u) {
        throw std::runtime_error("CreateTextureFromRGBA received too small RGBA buffer.");
    }
#if defined(RENDERING_PLANE_SAMPLE_D3D11)
    auto& backend = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend());
    return backend.createTextureFromRGBA(rgba.data(), width, height, width * 4u);
#else
    auto& backend = static_cast<VarjoXR::Backends::D3D12::D3D12Backend&>(space.backend());
    return backend.createTextureFromRGBA(rgba.data(), width, height, width * 4u);
#endif
}

inline double SecondsSinceStart() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto now = clock::now();
    return std::chrono::duration<double>(now - start).count();
}

inline void UpdateFrameContext(VarjoXR::XRSpace& space) {
    space.frameContext().timeSeconds = SecondsSinceStart();
    ++space.frameContext().frameNumber;
}

inline void RunForever(VarjoXR::XRSpace& space) {
    while (true) {
        UpdateFrameContext(space);
        space.update();
    }
}

inline std::string AnimatedCircleProcessingHlsl() {
    return R"hlsl(
Texture2D<float4> xrInput : register(t0);
RWTexture2D<float4> xrOutput : register(u0);

cbuffer CircleProcessingConstants : register(b0)
{
    float centerX;
    float centerY;
    float radius;
    float outsideBrightness;
    float edgeSoftness;
    float pulseStrength;
    float reserved0;
    float reserved1;
};

cbuffer XRTextureProcessingFrameConstants : register(b1)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    float4 frameParams;
};

float4 LoadNearest(float2 uv)
{
    const uint x = min((uint)(uv.x * (float)srcWidth), srcWidth - 1u);
    const uint y = min((uint)(uv.y * (float)srcHeight), srcHeight - 1u);
    return xrInput.Load(int3((int)x, (int)y, 0));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= dstWidth || id.y >= dstHeight) {
        return;
    }

    const float2 uv = (float2(id.xy) + 0.5f) / float2(dstWidth, dstHeight);
    float4 color = LoadNearest(uv);

    const float t = frameParams.z;
    const float animatedRadius = radius + sin(t * 6.2831853f) * pulseStrength;
    const float d = distance(uv, float2(centerX, centerY));
    const float soft = max(edgeSoftness, 1.0e-5f);
    const float outsideMask = smoothstep(animatedRadius - soft, animatedRadius + soft, d);
    color.rgb *= lerp(1.0f, outsideBrightness, outsideMask);

    xrOutput[id.xy] = color;
}
)hlsl";
}

inline VarjoXR::TextureProcessingDesc MakeAnimatedCircleProcessing(
    const CircleProcessingConstants& constants,
    glm::uvec2 outputSize) {
    VarjoXR::TextureProcessingDesc processing{};
    processing.enabled = true;
    processing.timing = VarjoXR::ProcessingTiming::BeforeRenderEachFrame;
    processing.hlsl = AnimatedCircleProcessingHlsl();
    processing.entryPoint = "main";
    processing.target = "cs_5_0";
    processing.sourceName = "RenderingPlane_06_ProcessingPlane.hlsl";
    processing.outputSize = outputSize;
    processing.userConstants.registerIndex = 0;
    processing.userConstants.set(constants);
    processing.frameConstants.enabled = true;
    processing.frameConstants.registerIndex = 1;
    return processing;
}

inline int ReportFailure(const char* sampleName, const std::exception& e) {
    std::cerr << sampleName << "_" << BackendName() << " failed: " << e.what() << '\n';
    return 1;
}

inline void PrintStart(const char* sampleName) {
    std::cout << sampleName << " running on " << BackendName() << ". Press Ctrl+C to exit.\n";
}

} // namespace RenderingPlaneSample
