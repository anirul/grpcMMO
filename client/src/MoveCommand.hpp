#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

namespace grpcmmo::client
{
struct MoveCommand
{
    glm::vec3 world_displacement_m = glm::vec3(0.0f);
    bool sprint = false;

    [[nodiscard]] bool HasTranslation() const
    {
        return glm::dot(world_displacement_m, world_displacement_m) > 0.000001f;
    }

    [[nodiscard]] bool HasSignal() const
    {
        return HasTranslation();
    }

    void Accumulate(const MoveCommand& other)
    {
        world_displacement_m += other.world_displacement_m;
        sprint = sprint || other.sprint;
    }

    void Clear()
    {
        *this = {};
    }
};
} // namespace grpcmmo::client
