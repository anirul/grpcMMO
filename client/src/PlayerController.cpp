#include "PlayerController.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Pawn.hpp"

namespace grpcmmo::client
{
namespace
{
glm::vec3 NormalizeOrFallback(
    const glm::vec3& direction, const glm::vec3& fallback
)
{
    const float length_squared = glm::dot(direction, direction);
    if (length_squared <= 0.000001f)
    {
        return fallback;
    }

    return direction / std::sqrt(length_squared);
}

glm::vec3 ProjectDirectionOntoSurface(
    const glm::vec3& direction,
    const glm::vec3& surface_up,
    const glm::vec3& fallback
)
{
    const glm::vec3 up =
        NormalizeOrFallback(surface_up, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 tangent = direction - (glm::dot(direction, up) * up);
    return NormalizeOrFallback(tangent, fallback);
}
} // namespace

bool PlayerController::FrameInput::HasGameplayInput() const
{
    return std::abs(move_forward) > 0.001f || std::abs(move_right) > 0.001f ||
           std::abs(look_yaw_delta_radians) > 0.0001f ||
           std::abs(look_pitch_delta_radians) > 0.0001f;
}

void PlayerController::Init()
{
    Controller::Init();
    ResetState();
    camera_boon_.Init();
    camera_boon_.Reset();
}

void PlayerController::End()
{
    camera_boon_.End();
    ResetState();
    Controller::End();
}

void PlayerController::Tick(float delta_seconds)
{
    Controller::Tick(delta_seconds);

    frame_input_ = {};
    frame_input_.move_forward = (move_forward_down_ ? 1.0f : 0.0f) -
                                (move_backward_down_ ? 1.0f : 0.0f);
    frame_input_.move_right =
        (move_right_down_ ? 1.0f : 0.0f) - (move_left_down_ ? 1.0f : 0.0f);
    frame_input_.exit_requested = exit_requested_;
    frame_input_.look_yaw_delta_radians =
        accumulated_mouse_delta_.x * kMouseYawSensitivity;
    frame_input_.look_pitch_delta_radians =
        accumulated_mouse_delta_.y * kMousePitchSensitivity;

    glm::vec2 movement(frame_input_.move_forward, frame_input_.move_right);
    if (glm::dot(movement, movement) > 1.0f)
    {
        movement = glm::normalize(movement);
        frame_input_.move_forward = movement.x;
        frame_input_.move_right = movement.y;
    }

    camera_boon_.Tick(delta_seconds);
    accumulated_mouse_delta_ = glm::vec2(0.0f);
    exit_requested_ = false;
}

PlayerController::FrameInput PlayerController::ConsumeFrameInput() const
{
    return frame_input_;
}

void PlayerController::DriveCamera(const FrameInput& frame_input)
{
    camera_boon_.AddYawDelta(frame_input.look_yaw_delta_radians);
    camera_boon_.AddPitchDelta(frame_input.look_pitch_delta_radians);
}

MoveCommand PlayerController::DrivePawn(
    const FrameInput& frame_input, float delta_seconds
)
{
    MoveCommand move_command;
    const Pawn* pawn = GetPawn();
    if (pawn == nullptr)
    {
        return move_command;
    }

    const float movement_step_seconds =
        std::clamp(delta_seconds, 0.0f, kMaxMovementStepSeconds);
    const bool has_translation_input =
        std::abs(frame_input.move_forward) > 0.001f ||
        std::abs(frame_input.move_right) > 0.001f;
    if (!has_translation_input || movement_step_seconds <= 0.0f)
    {
        return move_command;
    }

    const float movement_yaw_radians = camera_boon_.GetYawRadians();
    const glm::vec3 base_forward(
        std::cos(movement_yaw_radians), 0.0f, std::sin(movement_yaw_radians)
    );
    const glm::vec3 surface_up = pawn->GetSurfaceUp();
    const glm::vec3 forward = ProjectDirectionOntoSurface(
        base_forward, surface_up, pawn->GetRenderFacingDirection()
    );
    const glm::vec3 right = NormalizeOrFallback(
        glm::cross(forward, surface_up), glm::vec3(-forward.z, 0.0f, forward.x)
    );
    move_command.world_displacement_m =
        glm::dvec3(
            (forward * frame_input.move_forward) +
            (right * frame_input.move_right)
        ) *
        static_cast<double>(kMoveSpeedMetersPerSecond * movement_step_seconds);
    return move_command;
}

glm::vec3 PlayerController::GetLookFacingDirection() const
{
    const Pawn* pawn = GetPawn();
    const glm::vec3 base_forward(
        std::cos(camera_boon_.GetYawRadians()),
        0.0f,
        std::sin(camera_boon_.GetYawRadians())
    );
    if (pawn == nullptr)
    {
        return NormalizeOrFallback(base_forward, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    return ProjectDirectionOntoSurface(
        base_forward, pawn->GetSurfaceUp(), pawn->GetRenderFacingDirection()
    );
}

const CameraBoon& PlayerController::GetCameraBoon() const
{
    return camera_boon_;
}

bool PlayerController::KeyPressed(char key, double /*dt*/)
{
    if (key == 27)
    {
        exit_requested_ = true;
        return true;
    }

    const char lowered =
        static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    if (lowered == 'w')
    {
        move_forward_down_ = true;
    }
    else if (lowered == 's')
    {
        move_backward_down_ = true;
    }
    else if (lowered == 'a')
    {
        move_left_down_ = true;
    }
    else if (lowered == 'd')
    {
        move_right_down_ = true;
    }
    return true;
}

bool PlayerController::KeyReleased(char key, double /*dt*/)
{
    const char lowered =
        static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    if (lowered == 'w')
    {
        move_forward_down_ = false;
    }
    else if (lowered == 's')
    {
        move_backward_down_ = false;
    }
    else if (lowered == 'a')
    {
        move_left_down_ = false;
    }
    else if (lowered == 'd')
    {
        move_right_down_ = false;
    }
    return true;
}

bool PlayerController::
    MouseMoved(glm::vec2 /*position*/, glm::vec2 relative, double /*dt*/)
{
    if (!orbit_camera_left_down_ && !orbit_camera_right_down_)
    {
        return true;
    }
    accumulated_mouse_delta_ += relative;
    return true;
}

bool PlayerController::MousePressed(char button, double /*dt*/)
{
    if (button == kOrbitMouseButtonLeft)
    {
        orbit_camera_left_down_ = true;
    }
    if (button == kOrbitMouseButtonRight)
    {
        orbit_camera_right_down_ = true;
    }
    return true;
}

bool PlayerController::MouseReleased(char button, double /*dt*/)
{
    if (button == kOrbitMouseButtonLeft)
    {
        orbit_camera_left_down_ = false;
    }
    if (button == kOrbitMouseButtonRight)
    {
        orbit_camera_right_down_ = false;
    }
    return true;
}

bool PlayerController::WheelMoved(float /*relative*/, double /*dt*/)
{
    return true;
}

void PlayerController::NextFrame()
{
}

void PlayerController::ResetState()
{
    move_forward_down_ = false;
    move_backward_down_ = false;
    move_left_down_ = false;
    move_right_down_ = false;
    exit_requested_ = false;
    orbit_camera_left_down_ = false;
    orbit_camera_right_down_ = false;
    accumulated_mouse_delta_ = glm::vec2(0.0f);
    frame_input_ = {};
}
} // namespace grpcmmo::client
