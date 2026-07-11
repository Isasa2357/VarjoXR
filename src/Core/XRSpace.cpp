#include <VarjoXR/Core/XRSpace.hpp>

#include <sstream>
#include <stdexcept>
#include <utility>

#include <Varjo_mr.h>

#include <VarjoToolkit/Core/VarjoSession.hpp>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace {

thread_local const VarjoXR::XRSpace* g_renderThreadSpace = nullptr;

} // namespace

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

XRSpace::~XRSpace()
{
    stopRenderThread();
}

void XRSpace::ensureRenderThreadAccess(const char* operation) const
{
    if (renderThreadRunning_.load(std::memory_order_acquire) &&
        g_renderThreadSpace != this) {
        std::ostringstream message;
        message << operation
                << " cannot access render-owned XRSpace state from a non-render thread.";
        throw std::runtime_error(message.str());
    }
}

XRPlane& XRSpace::createPlane(glm::vec2 sizeMeters)
{
    ensureRenderThreadAccess("XRSpace::createPlane");
    auto plane = std::make_unique<XRPlane>(sizeMeters);
    XRPlane& reference = *plane;
    planes_.push_back(std::move(plane));
    return reference;
}

void XRSpace::clearPlanes()
{
    ensureRenderThreadAccess("XRSpace::clearPlanes");
    planes_.clear();
}

std::vector<std::unique_ptr<XRPlane>>& XRSpace::planes()
{
    ensureRenderThreadAccess("XRSpace::planes");
    return planes_;
}

const std::vector<std::unique_ptr<XRPlane>>& XRSpace::planes() const
{
    ensureRenderThreadAccess("XRSpace::planes const");
    return planes_;
}

FrameContext& XRSpace::frameContext()
{
    ensureRenderThreadAccess("XRSpace::frameContext");
    return frameContext_;
}

const FrameContext& XRSpace::frameContext() const
{
    ensureRenderThreadAccess("XRSpace::frameContext const");
    return frameContext_;
}

void XRSpace::beginFrame()
{
    ensureRenderThreadAccess("XRSpace::beginFrame");
    if (frameBegun_) {
        throw std::runtime_error(
            "XRSpace::beginFrame called while a frame is already active.");
    }
    backend_->beginFrame();
    frameBegun_ = true;
}

void XRSpace::render()
{
    ensureRenderThreadAccess("XRSpace::render");
    if (!frameBegun_) {
        throw std::runtime_error("XRSpace::render requires beginFrame first.");
    }
    backend_->render(planes_, frameContext_);
}

void XRSpace::endFrame()
{
    ensureRenderThreadAccess("XRSpace::endFrame");
    if (!frameBegun_) {
        throw std::runtime_error("XRSpace::endFrame requires beginFrame first.");
    }
    backend_->endFrame();
    frameBegun_ = false;
}

void XRSpace::update()
{
    ensureRenderThreadAccess("XRSpace::update");
    beginFrame();
    render();
    endFrame();
}

VarjoFrameInfoSnapshot XRSpace::frameInfoSnapshot() const
{
    if (asynchronousModeStarted_.load(std::memory_order_acquire)) {
        const auto snapshot = latestFrameInfoSnapshot();
        return snapshot ? *snapshot : VarjoFrameInfoSnapshot{};
    }
    return backend_->frameInfoSnapshot();
}

std::shared_ptr<const VarjoFrameInfoSnapshot>
XRSpace::latestFrameInfoSnapshot() const noexcept
{
    return frameInfoBuffer_.latest();
}

std::uint64_t XRSpace::frameInfoGeneration() const noexcept
{
    return frameInfoBuffer_.generation();
}

void XRSpace::publishAsyncRenderState(XRSpaceAsyncRenderState state)
{
    const auto current = asyncRenderStateBuffer_.latest();
    if (current && state.revision <= current->revision) {
        throw std::invalid_argument(
            "XRSpace::publishAsyncRenderState requires a monotonically increasing revision.");
    }
    asyncRenderStateBuffer_.publish(std::move(state));
}

std::shared_ptr<const XRSpaceAsyncRenderState>
XRSpace::latestAsyncRenderState() const noexcept
{
    return asyncRenderStateBuffer_.latest();
}

std::uint64_t XRSpace::asyncRenderStateGeneration() const noexcept
{
    return asyncRenderStateBuffer_.generation();
}

void XRSpace::applyLatestAsyncRenderState()
{
    const auto state = asyncRenderStateBuffer_.latest();
    if (!state) return;
    if (hasAppliedAsyncRenderState_ &&
        state->revision == appliedAsyncRenderStateRevision_) {
        return;
    }

    for (const auto& update : state->processingConstants) {
        if (update.planeIndex >= planes_.size()) {
            throw std::out_of_range(
                "XRSpace asynchronous processing-constant update references an invalid Plane index.");
        }
        planes_[update.planeIndex]
            ->material(update.eye)
            .processing
            .userConstants
            .data = update.data;
    }

    appliedAsyncRenderStateRevision_ = state->revision;
    hasAppliedAsyncRenderState_ = true;
}

void XRSpace::startRenderThread(XRSpaceRenderThreadDesc desc)
{
    if (renderThread_.joinable() ||
        renderThreadRunning_.load(std::memory_order_acquire)) {
        throw std::runtime_error(
            "XRSpace::startRenderThread called while a render thread already exists.");
    }
    if (frameBegun_) {
        throw std::runtime_error(
            "XRSpace::startRenderThread cannot start during an active frame.");
    }

    {
        std::lock_guard<std::mutex> lock(renderThreadExceptionMutex_);
        renderThreadException_ = nullptr;
    }

    renderThreadStopRequested_.store(false, std::memory_order_release);
    asynchronousModeStarted_.store(true, std::memory_order_release);
    renderThreadRunning_.store(true, std::memory_order_release);

    try {
        renderThread_ = std::thread(
            &XRSpace::renderThreadMain,
            this,
            std::move(desc));
    } catch (...) {
        renderThreadRunning_.store(false, std::memory_order_release);
        throw;
    }
}

void XRSpace::requestRenderThreadStop() noexcept
{
    renderThreadStopRequested_.store(true, std::memory_order_release);
}

void XRSpace::stopRenderThread() noexcept
{
    requestRenderThreadStop();
    if (renderThread_.joinable() &&
        renderThread_.get_id() != std::this_thread::get_id()) {
        try {
            renderThread_.join();
        } catch (...) {
        }
    }
}

bool XRSpace::renderThreadRunning() const noexcept
{
    return renderThreadRunning_.load(std::memory_order_acquire);
}

void XRSpace::setRenderThreadException(std::exception_ptr error) noexcept
{
    try {
        std::lock_guard<std::mutex> lock(renderThreadExceptionMutex_);
        renderThreadException_ = std::move(error);
    } catch (...) {
    }
}

void XRSpace::rethrowRenderThreadException() const
{
    std::exception_ptr error;
    {
        std::lock_guard<std::mutex> lock(renderThreadExceptionMutex_);
        error = renderThreadException_;
    }
    if (error) std::rethrow_exception(error);
}

void XRSpace::renderThreadMain(XRSpaceRenderThreadDesc desc) noexcept
{
    g_renderThreadSpace = this;

#if defined(_WIN32)
    if (desc.useHighThreadPriority) {
        if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)) {
            OutputDebugStringA(
                "[VarjoXR] Failed to set XRSpace render thread priority.\n");
        }
    }
#else
    (void)desc.useHighThreadPriority;
#endif

    try {
        while (!renderThreadStopRequested_.load(std::memory_order_acquire)) {
            applyLatestAsyncRenderState();
            if (desc.beforeFrame) desc.beforeFrame(*this);

            update();
            VarjoFrameInfoSnapshot snapshot = backend_->frameInfoSnapshot();
            if (!snapshot.valid) {
                throw std::runtime_error(
                    "XRSpace render thread received an invalid FrameInfo snapshot after update.");
            }

            frameInfoBuffer_.publish(snapshot);
            if (desc.afterFrame) desc.afterFrame(*this, snapshot);
        }
    } catch (...) {
        setRenderThreadException(std::current_exception());
        renderThreadStopRequested_.store(true, std::memory_order_release);
    }

    renderThreadRunning_.store(false, std::memory_order_release);
    g_renderThreadSpace = nullptr;
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
    ensureRenderThreadAccess("XRSpace::backend");
    return *backend_;
}

const IRenderBackend& XRSpace::backend() const
{
    ensureRenderThreadAccess("XRSpace::backend const");
    return *backend_;
}

} // namespace VarjoXR
