#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoXR/Backends/IRenderBackend.hpp>
#include <VarjoXR/Core/XRPlane.hpp>
#include <VarjoXR/Foundation/AsyncDoubleBuffer.hpp>
#include <VarjoXR/Foundation/Eye.hpp>
#include <VarjoXR/Foundation/FrameContext.hpp>

class VarjoSession;

namespace VarjoXR {

struct XRSpaceDesc {
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<IRenderBackend> backend;
};

class XRSpace;

struct XRPlaneProcessingConstantsUpdate {
    std::size_t planeIndex = 0;
    Eye eye = Eye::Left;
    std::vector<std::byte> data;
};

// A complete atomic batch of asynchronous render-state changes. The producer
// must increase revision whenever the batch changes. Left/right constants that
// must become visible together should be included in the same batch.
struct XRSpaceAsyncRenderState {
    std::uint64_t revision = 0;
    std::vector<XRPlaneProcessingConstantsUpdate> processingConstants;
};

struct XRSpaceRenderThreadDesc {
    // Runs on the dedicated render thread after the latest asynchronous state
    // has been applied and before XRSpace::update(). Typical use: consume the
    // latest camera D3D resource and assign it to a Plane.
    std::function<void(XRSpace&)> beforeFrame;

    // Runs on the dedicated render thread after a successful update and after
    // the same frame's FrameInfo snapshot has been published.
    std::function<void(XRSpace&, const VarjoFrameInfoSnapshot&)> afterFrame;

    bool useHighThreadPriority = true;
};

class XRSpace {
public:
    explicit XRSpace(XRSpaceDesc desc);
    ~XRSpace();

    XRSpace(const XRSpace&) = delete;
    XRSpace& operator=(const XRSpace&) = delete;

    XRPlane& createPlane(glm::vec2 sizeMeters = {1.0f, 1.0f});
    void clearPlanes();

    // Mutable access is only valid before startRenderThread(), or from one of
    // that render thread's callbacks. The referenced objects are render-thread
    // owned while asynchronous rendering is active.
    std::vector<std::unique_ptr<XRPlane>>& planes();
    const std::vector<std::unique_ptr<XRPlane>>& planes() const;

    FrameContext& frameContext();
    const FrameContext& frameContext() const;

    void beginFrame();
    void render();
    void endFrame();
    void update();

    // Synchronous mode: snapshot from the most recent beginFrame()/update().
    // Asynchronous mode: a copy of the latest snapshot published by the render
    // thread. This getter never performs another WaitSync.
    VarjoFrameInfoSnapshot frameInfoSnapshot() const;

    // Zero-additional-copy access to the asynchronous FrameInfo double buffer.
    // The returned immutable snapshot remains valid while the shared_ptr is held.
    std::shared_ptr<const VarjoFrameInfoSnapshot> latestFrameInfoSnapshot() const noexcept;
    std::uint64_t frameInfoGeneration() const noexcept;

    // Publishes a latest-only batch for the render thread. This is a
    // single-producer API; revisions must increase monotonically.
    void publishAsyncRenderState(XRSpaceAsyncRenderState state);
    std::shared_ptr<const XRSpaceAsyncRenderState> latestAsyncRenderState() const noexcept;
    std::uint64_t asyncRenderStateGeneration() const noexcept;

    // Starts a dedicated frame-pacing/render thread. XRSpace, backend, planes,
    // and frameContext become render-thread owned until stopRenderThread().
    void startRenderThread(XRSpaceRenderThreadDesc desc = {});
    void requestRenderThreadStop() noexcept;
    void stopRenderThread() noexcept;
    bool renderThreadRunning() const noexcept;
    void rethrowRenderThreadException() const;

    ::VarjoSession& session();
    const ::VarjoSession& session() const;
    IRenderBackend& backend();
    const IRenderBackend& backend() const;

private:
    void ensureRenderThreadAccess(const char* operation) const;
    void applyLatestAsyncRenderState();
    void renderThreadMain(XRSpaceRenderThreadDesc desc) noexcept;
    void setRenderThreadException(std::exception_ptr error) noexcept;

    std::shared_ptr<::VarjoSession> session_;
    std::unique_ptr<IRenderBackend> backend_;
    std::vector<std::unique_ptr<XRPlane>> planes_;
    FrameContext frameContext_{};
    bool frameBegun_ = false;

    AsyncDoubleBuffer<VarjoFrameInfoSnapshot> frameInfoBuffer_;
    AsyncDoubleBuffer<XRSpaceAsyncRenderState> asyncRenderStateBuffer_;
    std::uint64_t appliedAsyncRenderStateRevision_ = 0;
    bool hasAppliedAsyncRenderState_ = false;

    std::thread renderThread_;
    std::atomic<bool> renderThreadStopRequested_{false};
    std::atomic<bool> renderThreadRunning_{false};
    std::atomic<bool> asynchronousModeStarted_{false};

    mutable std::mutex renderThreadExceptionMutex_;
    std::exception_ptr renderThreadException_;
};

} // namespace VarjoXR
