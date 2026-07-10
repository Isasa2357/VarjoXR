#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>

// VarjoToolkit intentionally does not expose or call waitSync. The render
// backend is the single frame-pacing owner and performs the C API call here.
class VarjoXRRenderFrameInfo final : public VarjoFrameInfo {
public:
    using VarjoFrameInfo::VarjoFrameInfo;

    bool waitSyncFromRenderer()
    {
        if (!valid()) return false;
        varjo_WaitSync(session(), get());
        return true;
    }
};

// Reuse the existing backend implementation while replacing only its internal
// frame-info storage type and synchronization call at this translation-unit
// boundary. The original implementation is not compiled separately.
#define VarjoFrameInfo VarjoXRRenderFrameInfo
#define waitSync waitSyncFromRenderer
#include "D3D12Backend.cpp"
#undef waitSync
#undef VarjoFrameInfo

namespace VarjoXR::Backends::D3D12 {

VarjoFrameInfoSnapshot D3D12Backend::frameInfoSnapshot() const
{
    if (!impl_ || !impl_->frameInfo) return {};
    return impl_->frameInfo->snapshot();
}

} // namespace VarjoXR::Backends::D3D12
