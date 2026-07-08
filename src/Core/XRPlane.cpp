#include <VarjoXR/Core/XRPlane.hpp>

#include <utility>

namespace VarjoXR {

XRPlane::XRPlane(glm::vec2 sizeMeters)
    : sizeMeters_(sizeMeters) {}

void XRPlane::setTexture(std::shared_ptr<XRTexture> texture) {
    setTexture(Eye::Left, texture);
    setTexture(Eye::Right, std::move(texture));
}

void XRPlane::setTexture(Eye eye, std::shared_ptr<XRTexture> texture) {
    materials_[EyeIndex(eye)].texture = std::move(texture);
}

void XRPlane::setPixelShaderHLSL(const std::string& hlsl) {
    setPixelShaderHLSL(Eye::Left, hlsl);
    setPixelShaderHLSL(Eye::Right, hlsl);
}

void XRPlane::setPixelShaderHLSL(Eye eye, const std::string& hlsl) {
    materials_[EyeIndex(eye)].planePixelShaderHlsl = hlsl;
}

void XRPlane::setProcessing(const TextureProcessingDesc& processing) {
    setProcessing(Eye::Left, processing);
    setProcessing(Eye::Right, processing);
}

void XRPlane::setProcessing(Eye eye, const TextureProcessingDesc& processing) {
    materials_[EyeIndex(eye)].processing = processing;
}

void XRPlane::setTint(glm::vec4 tint) noexcept {
    setTint(Eye::Left, tint);
    setTint(Eye::Right, tint);
}

void XRPlane::setTint(Eye eye, glm::vec4 tint) noexcept {
    materials_[EyeIndex(eye)].tint = tint;
}

} // namespace VarjoXR
