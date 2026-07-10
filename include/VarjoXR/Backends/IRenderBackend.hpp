#pragma once

#include <memory>
#include <vector>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
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
    virtual void render(
        const std::vector<std::unique_ptr<XRPlane>>& planes,
        const FrameContext& frameContext) = 0;
    virtual void endFrame() = 0;

    // Returns the frame information captured by the backend's most recent
    // rendering-owned varjo_WaitSync call.
    virtual VarjoFrameInfoSnapshot frameInfoSnapshot() const = 0;

    void update(
        const std::vector<std::unique_ptr<XRPlane>>& planes,
        const FrameContext& frameContext)
    {
        beginFrame();
        render(planes, frameContext);
        endFrame();
    }
};

} // namespace VarjoXR
