#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include <VarjoXR/Core/XRMaterial.hpp>
#include <VarjoXR/Foundation/Eye.hpp>
#include <VarjoXR/Foundation/PlacementMode.hpp>
#include <VarjoXR/Foundation/Transform.hpp>

namespace VarjoXR {

class XRPlane {
public:
    explicit XRPlane(glm::vec2 sizeMeters = {1.0f, 1.0f});

    glm::vec2 size() const noexcept { return sizeMeters_; }
    void setSize(glm::vec2 sizeMeters) noexcept { sizeMeters_ = sizeMeters; }

    PlacementMode placementMode() const noexcept { return placementMode_; }
    void setPlacementMode(PlacementMode mode) noexcept { placementMode_ = mode; }

    Transform& transform() noexcept { return transform_; }
    const Transform& transform() const noexcept { return transform_; }

    XRMaterial& material(Eye eye) noexcept { return materials_[EyeIndex(eye)]; }
    const XRMaterial& material(Eye eye) const noexcept { return materials_[EyeIndex(eye)]; }

    void setTexture(std::shared_ptr<XRTexture> texture);
    void setTexture(Eye eye, std::shared_ptr<XRTexture> texture);

    void setPixelShaderHLSL(const std::string& hlsl);
    void setPixelShaderHLSL(Eye eye, const std::string& hlsl);

    void setProcessing(const TextureProcessingDesc& processing);
    void setProcessing(Eye eye, const TextureProcessingDesc& processing);

    void setTint(glm::vec4 tint) noexcept;
    void setTint(Eye eye, glm::vec4 tint) noexcept;

private:
    glm::vec2 sizeMeters_{1.0f, 1.0f};
    PlacementMode placementMode_ = PlacementMode::World;
    Transform transform_{};
    EyeArray<XRMaterial> materials_{};
};

} // namespace VarjoXR
