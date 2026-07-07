#include <VarjoXR/Detail/VarjoLayerSubmit.hpp>

#include <algorithm>
#include <stdexcept>

namespace VarjoXR::Detail {

VarjoLayerSubmit::~VarjoLayerSubmit() {
    if (m_frameInfo) {
        varjo_FreeFrameInfo(m_frameInfo);
        m_frameInfo = nullptr;
    }
}

void VarjoLayerSubmit::initialize(varjo_Session* session, const XRSpaceConfig& config) {
    if (!session) throw std::runtime_error("VarjoLayerSubmit::initialize: null session");

    m_config = config;
    m_viewCount = varjo_GetViewCount(session);
    if (m_viewCount <= 0) throw std::runtime_error("varjo_GetViewCount returned zero.");

    m_frameInfo = varjo_CreateFrameInfo(session);
    if (!m_frameInfo) throw std::runtime_error("varjo_CreateFrameInfo failed.");

    m_viewStates.resize(static_cast<size_t>(m_viewCount));
    m_layerViews.resize(static_cast<size_t>(m_viewCount));

    m_swapChainWidth = 1;
    m_swapChainHeight = 1;
    for (int32_t i = 0; i < m_viewCount; ++i) {
        const varjo_ViewDescription desc = varjo_GetViewDescription(session, i);
        auto& view = m_viewStates[static_cast<size_t>(i)];
        view.eye = (desc.eye == varjo_Eye_Right) ? Eye::Right : Eye::Left;
        view.preferredWidth = std::max(1, desc.width);
        view.preferredHeight = std::max(1, desc.height);
        m_swapChainWidth = std::max(m_swapChainWidth, view.preferredWidth);
        m_swapChainHeight = std::max(m_swapChainHeight, view.preferredHeight);
    }

    m_layer.header.type = varjo_LayerMultiProjType;
    m_layer.header.flags = m_config.layerFlags;
    m_layer.space = varjo_SpaceLocal;
    m_layer.viewCount = m_viewCount;
    m_layer.views = m_layerViews.data();
    m_layerHeader = reinterpret_cast<varjo_LayerHeader*>(&m_layer);
}

void VarjoLayerSubmit::waitSync(varjo_Session* session) {
    varjo_WaitSync(session, m_frameInfo);

    for (int32_t i = 0; i < m_viewCount; ++i) {
        const auto& src = m_frameInfo->views[i];
        auto& dst = m_viewStates[static_cast<size_t>(i)];
        dst.enabled = (src.enabled != 0);
        dst.preferredWidth = std::max(1, src.preferredWidth);
        dst.preferredHeight = std::max(1, src.preferredHeight);
        dst.projection = MakeMat4FromVarjoDoubleArray(src.projectionMatrix);
        dst.view = MakeMat4FromVarjoDoubleArray(src.viewMatrix);
    }
}

int64_t VarjoLayerSubmit::frameNumber() const noexcept {
    return m_frameInfo ? m_frameInfo->frameNumber : 0;
}

varjo_SwapChainConfig2 VarjoLayerSubmit::makeSwapChainConfig() const noexcept {
    varjo_SwapChainConfig2 cfg{};
    cfg.textureFormat = m_config.colorFormat;
    cfg.numberOfTextures = std::max(2, m_config.swapChainTextureCount);
    cfg.textureWidth = m_swapChainWidth;
    cfg.textureHeight = m_swapChainHeight;
    cfg.textureArraySize = std::max(1, m_viewCount);
    return cfg;
}

void VarjoLayerSubmit::fillLayerViews() {
    for (int32_t i = 0; i < m_viewCount; ++i) {
        const auto& view = m_viewStates[static_cast<size_t>(i)];
        auto& layerView = m_layerViews[static_cast<size_t>(i)];
        layerView.extension = nullptr;
        for (int j = 0; j < 16; ++j) {
            layerView.projection.value[j] = static_cast<double>(view.projection.m[static_cast<size_t>(j)]);
            layerView.view.value[j] = static_cast<double>(view.view.m[static_cast<size_t>(j)]);
        }
        layerView.viewport.swapChain = m_swapChain;
        layerView.viewport.x = 0;
        layerView.viewport.y = 0;
        layerView.viewport.width = view.preferredWidth;
        layerView.viewport.height = view.preferredHeight;
        layerView.viewport.arrayIndex = i;
        layerView.viewport.reserved = 0;
    }
}

varjo_SubmitInfoLayers VarjoLayerSubmit::makeSubmitInfo() {
    fillLayerViews();

    varjo_SubmitInfoLayers info{};
    info.frameNumber = frameNumber();
    info.layerCount = 1;
    info.layers = &m_layerHeader;
    return info;
}

} // namespace VarjoXR::Detail
