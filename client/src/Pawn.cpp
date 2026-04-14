#include "Pawn.hpp"

#include <cmath>

namespace grpcmmo::client
{
namespace
{
constexpr float kHardPositionCorrectionMeters = 1.25f;
constexpr float kSoftPositionCorrectionMeters = 0.35f;
constexpr float kIdleCorrectionDelaySeconds = 0.12f;
constexpr float kIdlePositionCorrectionRate = 7.0f;

glm::quat OrientationFromDirection(const glm::vec3& direction, const glm::quat& fallback)
{
    const glm::vec3 horizontal(direction.x, 0.0f, direction.z);
    const float horizontal_length_squared = glm::dot(horizontal, horizontal);
    if (horizontal_length_squared <= 0.000001f)
    {
        return fallback;
    }

    const float yaw_radians = -std::atan2(horizontal.z, horizontal.x);
    return glm::normalize(
        glm::angleAxis(yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 NormalizeFacingDirection(const glm::vec3& direction, const glm::vec3& fallback)
{
    const glm::vec3 horizontal(direction.x, 0.0f, direction.z);
    const float horizontal_length_squared = glm::dot(horizontal, horizontal);
    if (horizontal_length_squared <= 0.000001f)
    {
        return fallback;
    }

    return horizontal / std::sqrt(horizontal_length_squared);
}
} // namespace

void Pawn::Init()
{
    Actor::Init();
    SetEntityId({});
    SetDisplayName({});
    authoritative_position_ = glm::vec3(0.0f);
    authoritative_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
    predicted_position_ = glm::vec3(0.0f);
    predicted_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
    seconds_since_local_input_ = 1000.0f;
    controlled_ = false;
    initialized_ = false;
    SyncRootComponentFromRenderState();
}

void Pawn::End()
{
    Actor::End();
    authoritative_position_ = glm::vec3(0.0f);
    authoritative_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
    predicted_position_ = glm::vec3(0.0f);
    predicted_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
    seconds_since_local_input_ = 1000.0f;
    controlled_ = false;
    initialized_ = false;
}

void Pawn::Tick(float delta_seconds)
{
    Actor::Tick(delta_seconds);
    Reconcile(delta_seconds);
}

void Pawn::ApplyReplication(const PawnSnapshot& snapshot)
{
    SetEntityId(snapshot.pawn_id);
    SetDisplayName(snapshot.display_name);
    authoritative_position_ = snapshot.position;
    authoritative_facing_direction_ =
        NormalizeFacingDirection(snapshot.facing_direction, authoritative_facing_direction_);
    controlled_ = snapshot.controlled;

    if (!initialized_)
    {
        predicted_position_ = authoritative_position_;
        predicted_facing_direction_ = authoritative_facing_direction_;
        seconds_since_local_input_ = 1000.0f;
        initialized_ = true;
    }

    if (!controlled_)
    {
        predicted_position_ = authoritative_position_;
        predicted_facing_direction_ = authoritative_facing_direction_;
        seconds_since_local_input_ = 1000.0f;
    }

    SyncRootComponentFromRenderState();
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
        predicted_facing_direction_ = authoritative_facing_direction_;
        seconds_since_local_input_ = 1000.0f;
        return;
    }

    seconds_since_local_input_ += delta_seconds;
    const glm::vec3 position_error = authoritative_position_ - predicted_position_;
    const float position_error_m = glm::length(position_error);

    if (position_error_m > kHardPositionCorrectionMeters)
    {
        predicted_position_ = authoritative_position_;
    }
    else if (position_error_m > kSoftPositionCorrectionMeters &&
             seconds_since_local_input_ >= kIdleCorrectionDelaySeconds)
    {
        const float position_alpha =
            glm::clamp(delta_seconds * kIdlePositionCorrectionRate, 0.0f, 1.0f);
        predicted_position_ =
            glm::mix(predicted_position_, authoritative_position_, position_alpha);
    }

    SyncRootComponentFromRenderState();
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

    predicted_position_ += move_command.world_displacement_m;
    SyncRootComponentFromRenderState();
}

void Pawn::SetLocalFacingDirection(const glm::vec3& direction)
{
    if (!controlled_ || !initialized_)
    {
        return;
    }

    predicted_facing_direction_ =
        NormalizeFacingDirection(direction, predicted_facing_direction_);
    SyncRootComponentFromRenderState();
}

const std::string& Pawn::GetEntityId() const
{
    return Actor::GetEntityId();
}

glm::vec3 Pawn::GetRenderFacingDirection() const
{
    if (controlled_)
    {
        return predicted_facing_direction_;
    }
    return authoritative_facing_direction_;
}

glm::vec3 Pawn::GetRenderPosition() const
{
    return controlled_ ? predicted_position_ : authoritative_position_;
}

glm::quat Pawn::GetRenderOrientation() const
{
    return GetRootComponent().GetWorldOrientation();
}

float Pawn::GetRenderYawRadians() const
{
    const glm::vec3 facing_direction = GetRenderFacingDirection();
    return std::atan2(facing_direction.z, facing_direction.x);
}

bool Pawn::IsControlled() const
{
    return controlled_;
}

glm::vec3 Pawn::GetSurfaceUp() const
{
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

const char* Pawn::GetActorClassName() const
{
    return "Pawn";
}

void Pawn::SyncRootComponentFromRenderState()
{
    GetRootComponent().SetWorldPosition(GetRenderPosition());
    GetRootComponent().SetWorldOrientation(
        OrientationFromDirection(GetRenderFacingDirection(),
                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f)));
    GetRootComponent().SetWorldScale(glm::vec3(1.0f));
}
} // namespace grpcmmo::client
