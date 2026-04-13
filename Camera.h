#pragma once
#include <DirectXMath.h>

/// Third-person camera that orbits around a target position.
class Camera
{
public:
    float Pitch      =  0.35f;   // radians (vertical angle)
    float Yaw        =  0.0f;    // radians (horizontal angle)
    float Distance   = 12.0f;    // distance from target
    float FOV        = DirectX::XM_PIDIV4;
    float AspectRatio = 1.0f;
    float NearPlane  = 0.1f;
    float FarPlane   = 500.0f;

    /// Compute view matrix given a target position.
    DirectX::XMMATRIX GetView(DirectX::XMFLOAT3 target) const;

    /// Compute projection matrix.
    DirectX::XMMATRIX GetProjection() const;

    /// World-space position of the camera eye.
    DirectX::XMFLOAT3 GetEyePosition(DirectX::XMFLOAT3 target) const;
};
