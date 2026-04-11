#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace grpcmmo::client
{
struct MoveCommand
{
    glm::vec3 world_displacement_m = glm::vec3(0.0f);
    glm::quat facing_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool has_facing_orientation = false;
    bool sprint = false;

    [[nodiscard]] bool HasTranslation() const
    {
        return glm::dot(world_displacement_m, world_displacement_m) > 0.000001f;
    }

    [[nodiscard]] bool HasSignal() const
    {
        return HasTranslation() || has_facing_orientation;
    }

    void Accumulate(const MoveCommand& other)
    {
        world_displacement_m += other.world_displacement_m;
        if (other.has_facing_orientation)
        {
            facing_orientation = other.facing_orientation;
            has_facing_orientation = true;
        }
        sprint = sprint || other.sprint;
    }

    void SetFacingOrientation(const glm::quat& orientation)
    {
        facing_orientation = glm::normalize(orientation);
        has_facing_orientation = true;
    }

    void Clear()
    {
        *this = {};
    }
};
} // namespace grpcmmo::client
