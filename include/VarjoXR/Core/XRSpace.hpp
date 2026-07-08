#pragma once

#include <memory>
#include <vector>

#include <VarjoXR/Backends/IRenderBackend.hpp>
#include <VarjoXR/Core/XRPlane.hpp>
#include <VarjoXR/Foundation/FrameContext.hpp>

class VarjoSession;

namespace VarjoXR {

struct XRSpaceDesc {
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<IRenderBackend> backend;
};

class XRSpace {
public:
    explicit XRSpace(XRSpaceDesc desc);
    ~XRSpace();

    XRSpace(const XRSpace&) = delete;
    XRSpace& operator=(const XRSpace&) = delete;

    XRPlane& createPlane(glm::vec2 sizeMeters = {1.0f, 1.0f});
    void clearPlanes();

    std::vector<std::unique_ptr<XRPlane>>& planes() noexcept { return planes_; }
    const std::vector<std::unique_ptr<XRPlane>>& planes() const noexcept { return planes_; }

    FrameContext& frameContext() noexcept { return frameContext_; }
    const FrameContext& frameContext() const noexcept { return frameContext_; }

    void beginFrame();
    void render();
    void endFrame();
    void update();

    ::VarjoSession& session();
    const ::VarjoSession& session() const;
    IRenderBackend& backend();
    const IRenderBackend& backend() const;

private:
    std::shared_ptr<::VarjoSession> session_;
    std::unique_ptr<IRenderBackend> backend_;
    std::vector<std::unique_ptr<XRPlane>> planes_;
    FrameContext frameContext_{};
    bool frameBegun_ = false;
};

} // namespace VarjoXR
