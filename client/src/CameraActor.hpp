#pragma once

#include <string>

#include "Actor.hpp"
#include "Camera.hpp"

namespace grpcmmo::client
{
    class CameraActor final : public Actor
    {
      public:
        void SetCameraId(std::string camera_id)
        {
            camera_id_ = std::move(camera_id);
        }

        [[nodiscard]] const std::string& GetCameraId() const
        {
            return camera_id_;
        }

        void SetPose(const CameraPose& pose)
        {
            pose_ = pose;
            GetRootComponent().SetWorldPosition(pose.position);
        }

        [[nodiscard]] const CameraPose& GetPose() const
        {
            return pose_;
        }

        [[nodiscard]] const char* GetActorClassName() const override
        {
            return "CameraActor";
        }

      private:
        std::string camera_id_;
        CameraPose pose_{};
    };
} // namespace grpcmmo::client
