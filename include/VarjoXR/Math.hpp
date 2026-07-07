#pragma once

#include <array>

namespace VarjoXR {

struct Vec2 { float x = 0.0f; float y = 0.0f; };
struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
struct Vec4 { float x = 0.0f; float y = 0.0f; float z = 0.0f; float w = 1.0f; };

struct Mat4 {
    std::array<float, 16> m{};
    static Mat4 Identity() noexcept;
};

struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Mat4 matrix() const noexcept;
};

Mat4 MakeMat4FromVarjoDoubleArray(const double* m16) noexcept;

} // namespace VarjoXR
