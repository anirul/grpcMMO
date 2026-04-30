#include "Camera.hpp"

#include <algorithm>

#include "CameraBoon.hpp"
#include "frame/level_interface.h"
#include "frame/window_interface.h"

namespace grpcmmo::client
{
    void Camera::Init()
    {
        has_pose_ = false;
    }

    void Camera::End()
    {
        has_pose_ = false;
        window_ = nullptr;
    }

    void Camera::Tick(float /*delta_seconds*/)
    {
        if (window_ == nullptr || !has_pose_)
        {
            return;
        }

        auto& level = window_->GetDevice().GetLevel();
        auto& camera = level.GetDefaultCamera();
        camera.SetAspectRatio(
            static_cast<float>(window_->GetSize().x) /
            static_cast<float>(std::max(1u, window_->GetSize().y))
        );
        camera.SetPosition(pose_.position);
        camera.SetFront(glm::normalize(pose_.target - pose_.position));
        camera.SetUp(pose_.up);
    }

    void Camera::Attach(frame::WindowInterface* window)
    {
        window_ = window;
    }

    void Camera::SetPose(const CameraPose& pose)
    {
        pose_ = pose;
        has_pose_ = true;
    }
} // namespace grpcmmo::client
