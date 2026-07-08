#pragma once

#include <memory>
#include <vector>

#include <VarjoXR/Foundation/BackendType.hpp>
#include <VarjoXR/Foundation/FrameContext.hpp>

class VarjoSession;

namespace VarjoXR {

class XRPlane;

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual BackendType backendType() const noexcept = 0;

    virtual void initialize(std::shared_ptr<::VarjoSession> session) = 0;
    virtual void beginFrame() = 0;
    virtual void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) = 0;
    virtual void endFrame() = 0;

    void update(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) {
        beginFrame();
        render(planes, frameContext);
        endFrame();
    }
};

} // namespace VarjoXR
