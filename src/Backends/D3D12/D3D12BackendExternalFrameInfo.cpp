#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>

// VarjoToolkit intentionally does not expose or call waitSync. The D3D12
// backend is the single frame-pacing owner and performs the C API call here.
class VarjoXRD3D12RenderFrameInfo final : public VarjoFrameInfo {
public:
    using VarjoFrameInfo::VarjoFrameInfo;

    bool waitSyncFromRenderer()
    {
        if (!valid()) return false;
        varjo_WaitSync(session(), get());
        last_snapshot_ = snapshot();
        return last_snapshot_.valid;
    }

    VarjoFrameInfoSnapshot lastSnapshot() const
    {
        return last_snapshot_;
    }

private:
    VarjoFrameInfoSnapshot last_snapshot_{};
};

// Reuse the established backend implementation while replacing only its
// internal frame-info storage type and synchronization call in this translation
// unit. D3D12Backend.cpp is not compiled separately by CMake.
#define VarjoFrameInfo VarjoXRD3D12RenderFrameInfo
#define waitSync waitSyncFromRenderer
#include "D3D12Backend.cpp"
#undef waitSync
#undef VarjoFrameInfo

namespace VarjoXR::Backends::D3D12 {

VarjoFrameInfoSnapshot D3D12Backend::frameInfoSnapshot() const
{
    if (!impl_ || !impl_->frameInfo) return {};
    return impl_->frameInfo->lastSnapshot();
}

} // namespace VarjoXR::Backends::D3D12
