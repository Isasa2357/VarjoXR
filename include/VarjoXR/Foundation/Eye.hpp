#pragma once

#include <array>
#include <cstddef>

namespace VarjoXR {

enum class Eye : int {
    Left = 0,
    Right = 1,
};

inline constexpr int kEyeCount = 2;

constexpr std::size_t EyeIndex(Eye eye) noexcept {
    return static_cast<std::size_t>(eye);
}

template <typename T>
using EyeArray = std::array<T, kEyeCount>;

} // namespace VarjoXR
