#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>

class VarjoXRD3D11RenderFrameInfo final : public VarjoFrameInfo {
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

#define VarjoFrameInfo VarjoXRD3D11RenderFrameInfo
#define waitSync waitSyncFromRenderer
#include "D3D11Backend.cpp"
#undef waitSync
#undef VarjoFrameInfo

namespace VarjoXR::Backends::D3D11 {

VarjoFrameInfoSnapshot D3D11Backend::frameInfoSnapshot() const
{
    if (!impl_ || !impl_->frameInfo) return {};
    return impl_->frameInfo->lastSnapshot();
}

} // namespace VarjoXR::Backends::D3D11
