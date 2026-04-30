#include "CameraBoon.hpp"

#include <algorithm>

namespace grpcmmo::client
{
    namespace
    {
        constexpr float kPiRadians = 3.14159265358979323846f;

        float NormalizeAngleRadians(float angle_radians)
        {
            while (angle_radians > kPiRadians)
            {
                angle_radians -= 2.0f * kPiRadians;
            }
            while (angle_radians < -kPiRadians)
            {
                angle_radians += 2.0f * kPiRadians;
            }
            return angle_radians;
        }
    } // namespace

    void CameraBoon::Init()
    {
        Reset();
    }

    void CameraBoon::End()
    {
        Reset();
    }

    void CameraBoon::Tick(float /*delta_seconds*/)
    {
    }

    void CameraBoon::Reset()
    {
        yaw_radians_ = 0.0f;
        pitch_radians_ = 0.55f;
    }

    void CameraBoon::AddYawDelta(float yaw_delta_radians)
    {
        yaw_radians_ = NormalizeAngleRadians(yaw_radians_ + yaw_delta_radians);
    }

    void CameraBoon::AddPitchDelta(float pitch_delta_radians)
    {
        pitch_radians_ = std::clamp(
            pitch_radians_ + pitch_delta_radians,
            kPitchMinRadians,
            kPitchMaxRadians
        );
    }

    float CameraBoon::GetYawRadians() const
    {
        return yaw_radians_;
    }

    float CameraBoon::GetPitchRadians() const
    {
        return pitch_radians_;
    }

    float CameraBoon::GetDistanceMeters() const
    {
        return kDistanceMeters;
    }

    float CameraBoon::GetFocusHeightMeters() const
    {
        return kFocusHeightMeters;
    }
} // namespace grpcmmo::client
