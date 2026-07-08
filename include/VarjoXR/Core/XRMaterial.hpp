#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

#include <VarjoXR/Core/XRTexture.hpp>

namespace VarjoXR {

enum class ProcessingTiming {
    OnTextureChanged,
    BeforeRenderEachFrame,
};

struct TextureProcessingConstantBuffer {
    // Default convention: user constants are bound to b0.
    uint32_t registerIndex = 0;
    std::vector<std::byte> data;

    void setBytes(const void* src, std::size_t sizeBytes) {
        data.resize(sizeBytes);
        if (sizeBytes > 0) {
            std::memcpy(data.data(), src, sizeBytes);
        }
    }

    template <typename T>
    void set(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "TextureProcessingConstantBuffer::set requires a trivially copyable type.");
        setBytes(&value, sizeof(T));
    }
};

struct TextureProcessingFrameConstantsDesc {
    // VarjoXR-provided frame/texture constants are separated from user constants.
    // Default convention: b1.
    bool enabled = true;
    uint32_t registerIndex = 1;
};

// TextureProcessingDesc describes a programmable texture -> texture prepass.
// The application supplies a complete compute-shader HLSL source. The shader may
// include D3DHelper-provided HLSL library files, or any user-provided HLSL files
// available through includeDirs. VarjoXR compiles and dispatches the shader before
// drawing the Plane, then feeds the processed texture into the Plane HLSL.
//
// Expected D3D11/D3D12 resource binding convention for v0.1:
//   t0: input texture SRV
//   u0: output texture UAV
//   b0: user-defined constant buffer bytes, if userConstants.data is not empty
//   b1: optional VarjoXR frame/texture constants, if frameConstants.enabled
//
// Default thread group contract:
//   numthreads(8, 8, 1)
//   entryPoint defaults to "main".
struct TextureProcessingDesc {
    bool enabled = false;
    ProcessingTiming timing = ProcessingTiming::OnTextureChanged;

    // Complete compute-shader source. This is intentionally not a menu of built-in
    // operations; callers compose D3DHelper HLSL library functions or their own HLSL.
    std::string hlsl;
    std::string entryPoint = "main";
    std::string target = "cs_5_0";
    std::string sourceName = "VarjoXR_TextureProcessing.hlsl";
    std::vector<std::filesystem::path> includeDirs;

    // 0,0 means source texture size.
    glm::uvec2 outputSize{0, 0};

    TextureProcessingConstantBuffer userConstants;
    TextureProcessingFrameConstantsDesc frameConstants;
};

struct XRMaterial {
    std::shared_ptr<XRTexture> texture;
    TextureProcessingDesc processing;

    // Plane HLSL is the final pixel shader used while drawing the plane into the
    // Varjo swapchain. It is independent of the texture processing prepass, which
    // is a programmable texture -> texture compute pass.
    std::string planePixelShaderHlsl;

    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 params0{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 params1{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace VarjoXR
