# XRSpace asynchronous rendering

VarjoXR 0.3.0 adds a dedicated rendering thread to `XRSpace` while keeping the existing synchronous `update()` API.

## Ownership

After `startRenderThread()` succeeds, the following objects are owned by the render thread until `stopRenderThread()` completes:

- `XRSpace` frame execution
- render backend
- `XRPlane` objects and their materials
- `FrameContext`

Application code must not mutate these objects directly from another thread. Camera textures and other render-thread-only resources should be updated from `XRSpaceRenderThreadDesc::beforeFrame`.

## Frame loop

The dedicated thread executes this order for every frame:

```text
apply latest XRSpaceAsyncRenderState
beforeFrame callback
XRSpace::update
publish VarjoFrameInfoSnapshot
AfterFrame callback
```

`varjo_WaitSync` therefore remains inside the render thread and is still called only once by the selected backend.

## FrameInfo exchange

The render thread publishes immutable FrameInfo snapshots through an internal `AsyncDoubleBuffer`.

```cpp
const auto snapshot = space.latestFrameInfoSnapshot();
if (snapshot && snapshot->valid &&
    snapshot->frameNumber != lastFrameNumber) {
    eyeService.submitFrameInfo(*snapshot);
    imuService.submitFrameInfo(*snapshot);
    lastFrameNumber = snapshot->frameNumber;
}
```

This is latest-only exchange. A slow consumer may skip intermediate snapshots. Use a queue instead when every frame is required.

## Asynchronous HLSL constants

The application publishes a complete revisioned batch. Updates in one batch become visible together before the next render.

```cpp
VarjoXR::XRSpaceAsyncRenderState state;
state.revision = ++revision;

VarjoXR::XRPlaneProcessingConstantsUpdate left;
left.planeIndex = 0;
left.eye = VarjoXR::Eye::Left;
left.data = leftBytes;
state.processingConstants.push_back(std::move(left));

VarjoXR::XRPlaneProcessingConstantsUpdate right;
right.planeIndex = 0;
right.eye = VarjoXR::Eye::Right;
right.data = rightBytes;
state.processingConstants.push_back(std::move(right));

space.publishAsyncRenderState(std::move(state));
```

The producer must increase `revision` monotonically. The render thread retains the previous constants when no new revision has been published.

## Camera resource update

Use `beforeFrame` to consume the newest camera resource without involving the main thread.

```cpp
VarjoXR::XRSpaceRenderThreadDesc renderThread;
renderThread.beforeFrame = [&](VarjoXR::XRSpace& renderSpace) {
    if (auto latest = synchronizedFrames->tryPopLatest()) {
        // Upload or wrap D3D resources and assign them to a Plane here.
    }
};
renderThread.afterFrame = [&](VarjoXR::XRSpace&,
                              const VarjoFrameInfoSnapshot& snapshot) {
    // Render-thread-only fence bookkeeping may be performed here.
};

space.startRenderThread(std::move(renderThread));
```

## Error handling and shutdown

Exceptions from the render thread are stored instead of escaping the thread entry point.

```cpp
while (space.renderThreadRunning()) {
    space.rethrowRenderThreadException();
    // Main-thread work.
}

space.stopRenderThread();
space.rethrowRenderThreadException();
```

`stopRenderThread()` requests termination and joins the thread. `XRSpace` also calls it from its destructor.
