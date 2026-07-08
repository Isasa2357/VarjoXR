#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include <VarjoXR/Core/XRTexture.hpp>

namespace VarjoXR {

enum class ProcessingTiming {
    OnTextureChanged,
    BeforeRenderEachFrame,
};

// v0.1 keeps ImageProcessing as a small descriptor so that the public Material API
// is stable while D3D11/D3D12 implementations can map it to D3DHelper Processing.
// More complex graph/pipeline APIs can be added behind this descriptor later.
struct TextureProcessingDesc {
    bool enabled = false;
    ProcessingTiming timing = ProcessingTiming::OnTextureChanged;

    bool resizeEnabled = false;
    glm::uvec2 resizeSize{0, 0};

    bool blurEnabled = false;
    float blurRadius = 0.0f;

    bool regionDarkenEnabled = false;
    glm::vec2 regionCenterUv{0.5f, 0.5f};
    float regionRadiusUv = 0.25f;
    float outsideBrightness = 0.5f;

    bool customProcessingShaderEnabled = false;
    std::string customProcessingHlsl;
};

struct XRMaterial {
    std::shared_ptr<XRTexture> texture;
    TextureProcessingDesc processing;

    // Plane HLSL is the final pixel shader used while drawing the plane into the
    // Varjo swapchain. It is independent of ImageProcessing, which is texture -> texture.
    std::string planePixelShaderHlsl;

    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 params0{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 params1{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace VarjoXR
