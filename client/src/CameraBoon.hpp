#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "Object.hpp"

namespace grpcmmo::client
{
class CameraBoon : public Object
{
public:
    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    void Reset();
    void AddYawDelta(float yaw_delta_radians);
    void AddPitchDelta(float pitch_delta_radians);
    [[nodiscard]] float GetYawRadians() const;
    [[nodiscard]] float GetPitchRadians() const;
    [[nodiscard]] float GetDistanceMeters() const;
    [[nodiscard]] float GetFocusHeightMeters() const;

private:
    static constexpr float kDistanceMeters = 7.2f;
    static constexpr float kFocusHeightMeters = 0.70f;
    static constexpr float kPitchMinRadians = 0.20f;
    static constexpr float kPitchMaxRadians = 1.10f;

    float yaw_radians_ = 0.0f;
    float pitch_radians_ = 0.55f;
};
} // namespace grpcmmo::client
