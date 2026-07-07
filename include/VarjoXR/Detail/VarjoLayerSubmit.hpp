#pragma once

#include <VarjoXR/Eye.hpp>
#include <VarjoXR/Math.hpp>
#include <VarjoXR/XRSpace.hpp>

#include <Varjo.h>
#include <Varjo_layers.h>
#include <Varjo_types.h>
#include <Varjo_types_layers.h>

#include <cstdint>
#include <vector>

namespace VarjoXR::Detail {

struct VarjoViewState {
    Eye eye = Eye::Left;
    bool enabled = false;
    int32_t preferredWidth = 0;
    int32_t preferredHeight = 0;
    Mat4 projection;
    Mat4 view;
};

class VarjoLayerSubmit {
public:
    VarjoLayerSubmit() = default;
    ~VarjoLayerSubmit();

    VarjoLayerSubmit(const VarjoLayerSubmit&) = delete;
    VarjoLayerSubmit& operator=(const VarjoLayerSubmit&) = delete;

    void initialize(varjo_Session* session, const XRSpaceConfig& config);
    void waitSync(varjo_Session* session);

    int32_t viewCount() const noexcept { return m_viewCount; }
    int32_t swapChainWidth() const noexcept { return m_swapChainWidth; }
    int32_t swapChainHeight() const noexcept { return m_swapChainHeight; }
    int32_t swapChainArraySize() const noexcept { return m_viewCount; }
    int64_t frameNumber() const noexcept;

    const VarjoViewState& viewState(int32_t viewIndex) const { return m_viewStates.at(static_cast<size_t>(viewIndex)); }
    const std::vector<VarjoViewState>& viewStates() const noexcept { return m_viewStates; }

    varjo_SwapChainConfig2 makeSwapChainConfig() const noexcept;
    void setSwapChain(varjo_SwapChain* swapChain) noexcept { m_swapChain = swapChain; }
    varjo_SwapChain* swapChain() const noexcept { return m_swapChain; }

    void fillLayerViews();
    varjo_SubmitInfoLayers makeSubmitInfo();

private:
    varjo_FrameInfo* m_frameInfo = nullptr;
    varjo_SwapChain* m_swapChain = nullptr;
    XRSpaceConfig m_config{};
    int32_t m_viewCount = 0;
    int32_t m_swapChainWidth = 0;
    int32_t m_swapChainHeight = 0;

    std::vector<VarjoViewState> m_viewStates;
    std::vector<varjo_LayerMultiProjView> m_layerViews;
    varjo_LayerMultiProj m_layer{};
    varjo_LayerHeader* m_layerHeader = nullptr;
};

} // namespace VarjoXR::Detail
