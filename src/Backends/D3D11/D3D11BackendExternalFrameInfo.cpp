#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>

class VarjoXRD3D11RenderFrameInfo final : public VarjoFrameInfo {
public:
    using VarjoFrameInfo::VarjoFrameInfo;

    bool waitSyncFromRenderer()
    {
        if (!valid()) return false;
        varjo_WaitSync(session(), get());
        return true;
    }
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
    return impl_->frameInfo->snapshot();
}

} // namespace VarjoXR::Backends::D3D11
