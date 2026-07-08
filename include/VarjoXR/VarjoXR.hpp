#pragma once

#include <VarjoXR/Foundation/BackendType.hpp>
#include <VarjoXR/Foundation/Eye.hpp>
#include <VarjoXR/Foundation/FrameContext.hpp>
#include <VarjoXR/Foundation/PlacementMode.hpp>
#include <VarjoXR/Foundation/Transform.hpp>

#include <VarjoXR/Core/XRMaterial.hpp>
#include <VarjoXR/Core/XRPlane.hpp>
#include <VarjoXR/Core/XRSpace.hpp>
#include <VarjoXR/Core/XRTexture.hpp>

#if defined(VARJOXR_ENABLE_D3D11)
#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>
#endif

#if defined(VARJOXR_ENABLE_D3D12)
#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>
#endif
