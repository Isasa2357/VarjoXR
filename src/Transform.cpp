#include <VarjoXR/Math.hpp>

#include <DirectXMath.h>

namespace VarjoXR {

Mat4 Mat4::Identity() noexcept {
    Mat4 out{};
    out.m = {1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1};
    return out;
}

Mat4 Transform::matrix() const noexcept {
    using namespace DirectX;

    const XMMATRIX s = XMMatrixScaling(scale.x, scale.y, scale.z);
    const XMVECTOR q = XMVectorSet(rotation.x, rotation.y, rotation.z, rotation.w);
    const XMMATRIX r = XMMatrixRotationQuaternion(q);
    const XMMATRIX t = XMMatrixTranslation(position.x, position.y, position.z);
    const XMMATRIX m = XMMatrixTranspose(s * r * t);

    XMFLOAT4X4 f{};
    XMStoreFloat4x4(&f, m);

    Mat4 out{};
    const float* src = &f.m[0][0];
    for (int i = 0; i < 16; ++i) out.m[static_cast<size_t>(i)] = src[i];
    return out;
}

Mat4 MakeMat4FromVarjoDoubleArray(const double* m16) noexcept {
    Mat4 out{};
    if (!m16) return Mat4::Identity();
    for (int i = 0; i < 16; ++i) out.m[static_cast<size_t>(i)] = static_cast<float>(m16[i]);
    return out;
}

} // namespace VarjoXR
