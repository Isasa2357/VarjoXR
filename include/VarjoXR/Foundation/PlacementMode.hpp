#pragma once

namespace VarjoXR {

// World: object transform is interpreted in Varjo local/world space.
// HeadRelative: object transform is interpreted relative to the frame-synchronized HMD pose.
// The head-relative matrix must be computed during render() from the same frame information
// used for view/projection submission to avoid visible jitter.
enum class PlacementMode {
    World,
    HeadRelative,
};

} // namespace VarjoXR
