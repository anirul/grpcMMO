#include "Pawn.hpp"

#include <cmath>

namespace grpcmmo::client
{
namespace
{
constexpr float kPiRadians = 3.14159265358979323846f;
constexpr float kHardPositionCorrectionMeters = 1.25f;
constexpr float kHardYawCorrectionRadians = 0.55f;
constexpr float kIdleCorrectionDelaySeconds = 0.12f;
constexpr float kIdlePositionCorrectionRate = 7.0f;
constexpr float kIdleYawCorrectionRate = 10.0f;
}

void Pawn::Init()
{
    entity_id_.clear();
    display_name_.clear();
    authoritative_position_ = glm::vec3(0.0f);
    authoritative_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    predicted_position_ = glm::vec3(0.0f);
    authoritative_yaw_radians_ = 0.0f;
    predicted_yaw_radians_ = 0.0f;
    seconds_since_local_input_ = 1000.0f;
    controlled_ = false;
    initialized_ = false;
}

void Pawn::End()
{
    Init();
}

void Pawn::Tick(float delta_seconds)
{
    Reconcile(delta_seconds);
}

void Pawn::ApplyReplication(const PawnSnapshot& snapshot)
{
    entity_id_ = snapshot.pawn_id;
    display_name_ = snapshot.display_name;
    authoritative_position_ = snapshot.position;
    authoritative_orientation_ = snapshot.orientation;
    authoritative_yaw_radians_ = ExtractYawRadians(snapshot.orientation);
    controlled_ = snapshot.controlled;

    if (!initialized_)
    {
        predicted_position_ = authoritative_position_;
        predicted_yaw_radians_ = authoritative_yaw_radians_;
        seconds_since_local_input_ = 1000.0f;
        initialized_ = true;
    }

    if (!controlled_)
    {
        predicted_position_ = authoritative_position_;
        predicted_yaw_radians_ = authoritative_yaw_radians_;
        seconds_since_local_input_ = 1000.0f;
    }
}

void Pawn::Reconcile(float delta_seconds)
{
    if (!initialized_)
    {
        return;
    }

    if (!controlled_)
    {
        predicted_position_ = authoritative_position_;
        predicted_yaw_radians_ = authoritative_yaw_radians_;
        seconds_since_local_input_ = 1000.0f;
        return;
    }

    seconds_since_local_input_ += delta_seconds;
    const glm::vec3 position_error = authoritative_position_ - predicted_position_;
    const float position_error_m = glm::length(position_error);
    const float yaw_error =
        NormalizeAngleRadians(authoritative_yaw_radians_ - predicted_yaw_radians_);

    if (position_error_m > kHardPositionCorrectionMeters)
    {
        predicted_position_ = authoritative_position_;
    }
    else if (seconds_since_local_input_ >= kIdleCorrectionDelaySeconds)
    {
        const float position_alpha =
            glm::clamp(delta_seconds * kIdlePositionCorrectionRate, 0.0f, 1.0f);
        predicted_position_ =
            glm::mix(predicted_position_, authoritative_position_, position_alpha);
    }

    if (std::abs(yaw_error) > kHardYawCorrectionRadians)
    {
        predicted_yaw_radians_ = authoritative_yaw_radians_;
    }
    else if (seconds_since_local_input_ >= kIdleCorrectionDelaySeconds)
    {
        const float yaw_alpha =
            glm::clamp(delta_seconds * kIdleYawCorrectionRate, 0.0f, 1.0f);
        predicted_yaw_radians_ =
            NormalizeAngleRadians(predicted_yaw_radians_ + (yaw_error * yaw_alpha));
    }
}

void Pawn::ApplyMove(const MoveCommand& move_command)
{
    if (!controlled_ || !initialized_)
    {
        return;
    }

    if (move_command.HasSignal())
    {
        seconds_since_local_input_ = 0.0f;
    }

    if (!move_command.HasTranslation())
    {
        return;
    }

    predicted_yaw_radians_ = NormalizeAngleRadians(
        std::atan2(move_command.world_displacement_m.z, move_command.world_displacement_m.x));
    predicted_position_ += move_command.world_displacement_m;
}

void Pawn::SetLocalFacingYaw(float yaw_radians)
{
    if (!controlled_ || !initialized_)
    {
        return;
    }

    predicted_yaw_radians_ = NormalizeAngleRadians(yaw_radians);
}

const std::string& Pawn::GetEntityId() const
{
    return entity_id_;
}

glm::vec3 Pawn::GetRenderPosition() const
{
    return controlled_ ? predicted_position_ : authoritative_position_;
}

glm::quat Pawn::GetRenderOrientation() const
{
    if (controlled_)
    {
        return glm::angleAxis(predicted_yaw_radians_, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    return authoritative_orientation_;
}

float Pawn::GetRenderYawRadians() const
{
    return controlled_ ? predicted_yaw_radians_ : authoritative_yaw_radians_;
}

bool Pawn::IsControlled() const
{
    return controlled_;
}

glm::vec3 Pawn::GetSurfaceUp() const
{
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

float Pawn::ExtractYawRadians(const glm::quat& orientation)
{
    glm::vec3 forward = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));
    forward.y = 0.0f;
    if (glm::dot(forward, forward) < 0.0001f)
    {
        return 0.0f;
    }
    forward = glm::normalize(forward);
    return std::atan2(forward.z, forward.x);
}

float Pawn::NormalizeAngleRadians(float angle_radians)
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
} // namespace grpcmmo::client
