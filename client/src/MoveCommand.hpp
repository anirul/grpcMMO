#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace grpcmmo::client
{
struct MoveCommand
{
    glm::vec3 world_displacement_m = glm::vec3(0.0f);
    glm::vec3 facing_direction_unit = glm::vec3(1.0f, 0.0f, 0.0f);
    bool has_facing_direction = false;
    bool sprint = false;

    [[nodiscard]] bool HasTranslation() const
    {
        return glm::dot(world_displacement_m, world_displacement_m) > 0.000001f;
    }

    [[nodiscard]] bool HasSignal() const
    {
        return HasTranslation() || has_facing_direction;
    }

    void Accumulate(const MoveCommand& other)
    {
        world_displacement_m += other.world_displacement_m;
        if (other.has_facing_direction)
        {
            facing_direction_unit = other.facing_direction_unit;
            has_facing_direction = true;
        }
        sprint = sprint || other.sprint;
    }

    void SetFacingDirection(const glm::vec3& direction)
    {
        const glm::vec3 horizontal(direction.x, 0.0f, direction.z);
        const float length_squared = glm::dot(horizontal, horizontal);
        if (length_squared <= 0.000001f)
        {
            return;
        }

        facing_direction_unit = horizontal / std::sqrt(length_squared);
        has_facing_direction = true;
    }

    void Clear()
    {
        *this = {};
    }
};
} // namespace grpcmmo::client
