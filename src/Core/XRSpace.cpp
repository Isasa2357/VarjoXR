#include <VarjoXR/Core/XRSpace.hpp>

#include <stdexcept>
#include <utility>

#include <Varjo_mr.h>

#include <VarjoToolkit/Core/VarjoSession.hpp>

namespace VarjoXR {

XRSpace::XRSpace(XRSpaceDesc desc)
    : session_(std::move(desc.session))
    , backend_(std::move(desc.backend))
{
    if (!session_) {
        throw std::runtime_error(
            "XRSpace requires an external VarjoToolkit::VarjoSession.");
    }
    if (!session_->valid()) {
        throw std::runtime_error(
            "XRSpace requires a valid VarjoToolkit::VarjoSession.");
    }
    if (!backend_) {
        throw std::runtime_error("XRSpace requires an external render backend.");
    }

    varjo_MRSetVideoRender(session_->get(), varjo_True);
    varjo_MRSetVRViewOffset(session_->get(), 1.0);
    (void)varjo_GetError(session_->get());

    backend_->initialize(session_);
}

XRSpace::~XRSpace() = default;

XRPlane& XRSpace::createPlane(glm::vec2 sizeMeters)
{
    auto plane = std::make_unique<XRPlane>(sizeMeters);
    XRPlane& reference = *plane;
    planes_.push_back(std::move(plane));
    return reference;
}

void XRSpace::clearPlanes()
{
    planes_.clear();
}

void XRSpace::beginFrame()
{
    if (frameBegun_) {
        throw std::runtime_error(
            "XRSpace::beginFrame called while a frame is already active.");
    }
    backend_->beginFrame();
    frameBegun_ = true;
}

void XRSpace::render()
{
    if (!frameBegun_) {
        throw std::runtime_error("XRSpace::render requires beginFrame first.");
    }
    backend_->render(planes_, frameContext_);
}

void XRSpace::endFrame()
{
    if (!frameBegun_) {
        throw std::runtime_error("XRSpace::endFrame requires beginFrame first.");
    }
    backend_->endFrame();
    frameBegun_ = false;
}

void XRSpace::update()
{
    beginFrame();
    render();
    endFrame();
}

VarjoFrameInfoSnapshot XRSpace::frameInfoSnapshot() const
{
    return backend_->frameInfoSnapshot();
}

::VarjoSession& XRSpace::session()
{
    return *session_;
}

const ::VarjoSession& XRSpace::session() const
{
    return *session_;
}

IRenderBackend& XRSpace::backend()
{
    return *backend_;
}

const IRenderBackend& XRSpace::backend() const
{
    return *backend_;
}

} // namespace VarjoXR
