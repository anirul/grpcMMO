#include "Controller.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Pawn.hpp"

namespace grpcmmo::client
{
namespace
{
constexpr float kWalkSpeedMetersPerSecond = 4.0f;
constexpr float kSprintSpeedMetersPerSecond = 7.0f;
constexpr float kMaxMovementStepSeconds = 0.05f;
} // namespace

bool Controller::FrameInput::HasGameplayInput() const
{
    return std::abs(move_forward) > 0.001f ||
           std::abs(move_right) > 0.001f ||
           std::abs(look_yaw_delta_radians) > 0.0001f ||
           std::abs(look_pitch_delta_radians) > 0.0001f;
}

void Controller::Init()
{
    ResetState();
}

void Controller::End()
{
    ResetState();
}

void Controller::Tick(float /*delta_seconds*/)
{
    frame_input_ = {};
    frame_input_.move_forward =
        (move_forward_down_ ? 1.0f : 0.0f) - (move_backward_down_ ? 1.0f : 0.0f);
    frame_input_.move_right =
        (move_right_down_ ? 1.0f : 0.0f) - (move_left_down_ ? 1.0f : 0.0f);
    frame_input_.sprint = sprint_down_;
    frame_input_.exit_requested = exit_requested_;
    frame_input_.look_yaw_delta_radians =
        -accumulated_mouse_delta_.x * kMouseYawSensitivity;
    frame_input_.look_pitch_delta_radians =
        -accumulated_mouse_delta_.y * kMousePitchSensitivity;

    glm::vec2 movement(frame_input_.move_forward, frame_input_.move_right);
    if (glm::dot(movement, movement) > 1.0f)
    {
        movement = glm::normalize(movement);
        frame_input_.move_forward = movement.x;
        frame_input_.move_right = movement.y;
    }

    accumulated_mouse_delta_ = glm::vec2(0.0f);
    exit_requested_ = false;
}

Controller::FrameInput Controller::ConsumeFrameInput()
{
    return frame_input_;
}

void Controller::DriveCamera(CameraBoon& camera_boon, const FrameInput& frame_input) const
{
    camera_boon.AddYawDelta(frame_input.look_yaw_delta_radians);
    camera_boon.AddPitchDelta(frame_input.look_pitch_delta_radians);
}

MoveCommand Controller::DrivePawn(Pawn& pawn,
                                  const CameraBoon& camera_boon,
                                  const FrameInput& frame_input,
                                  float delta_seconds) const
{
    MoveCommand move_command;
    const float movement_step_seconds =
        std::clamp(delta_seconds, 0.0f, kMaxMovementStepSeconds);
    const bool has_translation_input =
        std::abs(frame_input.move_forward) > 0.001f ||
        std::abs(frame_input.move_right) > 0.001f;
    if (!has_translation_input || movement_step_seconds <= 0.0f)
    {
        return move_command;
    }

    move_command.sprint = frame_input.sprint;

    const float speed =
        frame_input.sprint ? kSprintSpeedMetersPerSecond : kWalkSpeedMetersPerSecond;
    const float movement_yaw_radians = camera_boon.GetYawRadians();
    const glm::vec3 forward(std::cos(movement_yaw_radians),
                            0.0f,
                            std::sin(movement_yaw_radians));
    const glm::vec3 right(-forward.z, 0.0f, forward.x);
    move_command.world_displacement_m =
        ((forward * frame_input.move_forward) + (right * frame_input.move_right)) *
        speed * movement_step_seconds;

    pawn.ApplyMove(move_command);
    return move_command;
}

bool Controller::KeyPressed(char key, double /*dt*/)
{
    if (key == frame::KEY_LSHIFT || key == frame::KEY_RSHIFT)
    {
        sprint_down_ = true;
        return true;
    }
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

bool Controller::KeyReleased(char key, double /*dt*/)
{
    if (key == frame::KEY_LSHIFT || key == frame::KEY_RSHIFT)
    {
        sprint_down_ = false;
        return true;
    }

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

bool Controller::MouseMoved(glm::vec2 /*position*/,
                            glm::vec2 relative,
                            double /*dt*/)
{
    if (!orbit_camera_left_down_ && !orbit_camera_right_down_)
    {
        return true;
    }
    accumulated_mouse_delta_ += relative;
    return true;
}

bool Controller::MousePressed(char button, double /*dt*/)
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

bool Controller::MouseReleased(char button, double /*dt*/)
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

bool Controller::WheelMoved(float /*relative*/, double /*dt*/)
{
    return true;
}

void Controller::NextFrame()
{
}

void Controller::ResetState()
{
    move_forward_down_ = false;
    move_backward_down_ = false;
    move_left_down_ = false;
    move_right_down_ = false;
    sprint_down_ = false;
    exit_requested_ = false;
    orbit_camera_left_down_ = false;
    orbit_camera_right_down_ = false;
    accumulated_mouse_delta_ = glm::vec2(0.0f);
    frame_input_ = {};
}
} // namespace grpcmmo::client
