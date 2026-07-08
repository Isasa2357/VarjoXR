#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace VarjoXR {

// Dynamic values supplied by the application for the frame currently being rendered.
// Values such as gaze position, time, and arbitrary shader parameters should be
// updated after beginFrame() and before render(), so that the data is temporally
// aligned with the Varjo frame information used for rendering and submission.
struct FrameContext {
    double timeSeconds = 0.0;
    int64_t frameNumber = 0;
    glm::vec2 gazeUv{0.5f, 0.5f};
    glm::vec4 params0{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 params1{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace VarjoXR
