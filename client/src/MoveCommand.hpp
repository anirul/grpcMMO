#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace grpcmmo::client
{
struct MoveCommand
{
    glm::dvec3 world_displacement_m = glm::dvec3(0.0);
    glm::dvec3 facing_direction_unit = glm::dvec3(1.0, 0.0, 0.0);
    bool has_facing_direction = false;

    [[nodiscard]] bool HasTranslation() const
    {
        return glm::dot(world_displacement_m, world_displacement_m) > 0.000001;
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
    }

    void SetFacingDirection(const glm::dvec3& direction)
    {
        const double length_squared = glm::dot(direction, direction);
        if (length_squared <= 0.000001)
        {
            return;
        }

        facing_direction_unit = direction / std::sqrt(length_squared);
        has_facing_direction = true;
    }

    void Clear()
    {
        *this = {};
    }
};
} // namespace grpcmmo::client
