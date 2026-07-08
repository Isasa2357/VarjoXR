#pragma once

#include <VarjoXR/VarjoXR.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ProgrammableProcessingSample {

struct CircleDarkenConstants {
    float centerX = 0.5f;
    float centerY = 0.5f;
    float radius = 0.28f;
    float outsideBrightness = 0.45f;

    float edgeSoftness = 0.03f;
    float pulseStrength = 0.04f;
    float reserved0 = 0.0f;
    float reserved1 = 0.0f;
};

inline std::string LoadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open text file: " + path.string());
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
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

inline std::filesystem::path ShaderDirectory() {
#ifdef VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR
    return std::filesystem::path(VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR);
#else
    return std::filesystem::path(".");
#endif
}

inline std::filesystem::path VarjoXrHlslDirectory() {
#ifdef VARJOXR_HLSL_DIR
    return std::filesystem::path(VARJOXR_HLSL_DIR);
#else
    return std::filesystem::path(".");
#endif
}

inline VarjoXR::TextureProcessingDesc MakeCircleDarkenProcessing(
    const std::filesystem::path& shaderDirectory,
    const CircleDarkenConstants& constants,
    glm::uvec2 outputSize) {
    VarjoXR::TextureProcessingDesc processing{};
    processing.enabled = true;
    processing.timing = VarjoXR::ProcessingTiming::BeforeRenderEachFrame;
    processing.hlsl = LoadTextFile(shaderDirectory / "CircleDarkenPreprocess.hlsl");
    processing.entryPoint = "main";
    processing.target = "cs_5_0";
    processing.sourceName = "CircleDarkenPreprocess.hlsl";
    processing.includeDirs.push_back(VarjoXrHlslDirectory());
    processing.outputSize = outputSize;

    processing.userConstants.registerIndex = 0; // b0
    processing.userConstants.set(constants);

    processing.frameConstants.enabled = true;
    processing.frameConstants.registerIndex = 1; // b1
    return processing;
}

inline double SecondsSinceStart() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto now = clock::now();
    return std::chrono::duration<double>(now - start).count();
}

} // namespace ProgrammableProcessingSample
