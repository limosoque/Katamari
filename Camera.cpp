#include "Camera.h"
#include <cmath>

using namespace DirectX;

XMMATRIX Camera::GetView(XMFLOAT3 target) const
{
    XMFLOAT3 eye = GetEyePosition(target);
    XMVECTOR eyeV    = XMLoadFloat3(&eye);
    XMVECTOR targetV = XMLoadFloat3(&target);
    XMVECTOR up      = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(eyeV, targetV, up);
}

XMMATRIX Camera::GetProjection() const
{
    return XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
}

XMFLOAT3 Camera::GetEyePosition(XMFLOAT3 target) const
{
    // Spherical coordinates around target
    float x = Distance * std::cos(Pitch) * std::sin(Yaw);
    float y = Distance * std::sin(Pitch);
    float z = Distance * std::cos(Pitch) * std::cos(Yaw);
    return { target.x + x, target.y + y, target.z + z };
}
