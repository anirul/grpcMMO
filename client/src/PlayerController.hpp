#pragma once

#include "frame/api.h"
#include "frame/input_interface.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "CameraBoon.hpp"
#include "Controller.hpp"
#include "MoveCommand.hpp"

namespace grpcmmo::client
{
class PlayerController : public Controller, public frame::InputInterface
{
public:
    struct FrameInput
    {
        float move_forward = 0.0f;
        float move_right = 0.0f;
        float look_yaw_delta_radians = 0.0f;
        float look_pitch_delta_radians = 0.0f;
        bool exit_requested = false;

        [[nodiscard]] bool HasGameplayInput() const;
    };

    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    [[nodiscard]] FrameInput ConsumeFrameInput() const;
    void DriveCamera(const FrameInput& frame_input);
    [[nodiscard]] MoveCommand DrivePawn(const FrameInput& frame_input,
                                        float delta_seconds);
    [[nodiscard]] glm::vec3 GetLookFacingDirection() const;
    [[nodiscard]] const CameraBoon& GetCameraBoon() const;

    bool KeyPressed(char key, double dt) override;
    bool KeyReleased(char key, double dt) override;
    bool MouseMoved(glm::vec2 position, glm::vec2 relative, double dt) override;
    bool MousePressed(char button, double dt) override;
    bool MouseReleased(char button, double dt) override;
    bool WheelMoved(float relative, double dt) override;
    void NextFrame() override;

private:
    void ResetState();

    static constexpr float kMoveSpeedMetersPerSecond = 4.0f;
    static constexpr float kMaxMovementStepSeconds = 0.05f;
    static constexpr float kMouseYawSensitivity = 0.0045f;
    static constexpr float kMousePitchSensitivity = 0.0030f;
    static constexpr char kOrbitMouseButtonLeft = 19;
    static constexpr char kOrbitMouseButtonRight = 23;

    CameraBoon camera_boon_{};
    bool move_forward_down_ = false;
    bool move_backward_down_ = false;
    bool move_left_down_ = false;
    bool move_right_down_ = false;
    bool exit_requested_ = false;
    bool orbit_camera_left_down_ = false;
    bool orbit_camera_right_down_ = false;
    glm::vec2 accumulated_mouse_delta_ = glm::vec2(0.0f);
    FrameInput frame_input_{};
};
} // namespace grpcmmo::client
