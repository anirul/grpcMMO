#pragma once

#include "Object.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

namespace frame
{
    class WindowInterface;
}

namespace grpcmmo::client
{
    struct CameraPose
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    };

    class Camera : public Object
    {
      public:
        void Init() override;
        void End() override;
        void Tick(float delta_seconds) override;

        void Attach(frame::WindowInterface* window);
        void SetPose(const CameraPose& pose);

      private:
        frame::WindowInterface* window_ = nullptr;
        CameraPose pose_{};
        bool has_pose_ = false;
    };
} // namespace grpcmmo::client
