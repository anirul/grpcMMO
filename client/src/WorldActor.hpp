#pragma once

#include <string>

#include "Actor.hpp"

namespace grpcmmo::client
{
class WorldActor : public Actor
{
public:
    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "WorldActor";
    }
};

class PlanetActor final : public WorldActor
{
public:
    void SetPlanetId(std::string planet_id)
    {
        planet_id_ = std::move(planet_id);
    }

    [[nodiscard]] const std::string& GetPlanetId() const
    {
        return planet_id_;
    }

    void SetZoneId(std::string zone_id)
    {
        zone_id_ = std::move(zone_id);
    }

    [[nodiscard]] const std::string& GetZoneId() const
    {
        return zone_id_;
    }

    void SetPatchId(std::string patch_id)
    {
        patch_id_ = std::move(patch_id);
    }

    [[nodiscard]] const std::string& GetPatchId() const
    {
        return patch_id_;
    }

    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "PlanetActor";
    }

private:
    std::string planet_id_ = "planet-dev";
    std::string zone_id_ = "zone-dev";
    std::string patch_id_ = "patch-000";
};

class StaticPropActor final : public WorldActor
{
public:
    void SetArchetypeId(std::string archetype_id)
    {
        archetype_id_ = std::move(archetype_id);
    }

    [[nodiscard]] const std::string& GetArchetypeId() const
    {
        return archetype_id_;
    }

    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "StaticPropActor";
    }

private:
    std::string archetype_id_ = "static_prop";
};

class InteractivePropActor final : public WorldActor
{
public:
    void SetArchetypeId(std::string archetype_id)
    {
        archetype_id_ = std::move(archetype_id);
    }

    [[nodiscard]] const std::string& GetArchetypeId() const
    {
        return archetype_id_;
    }

    void SetAnimationState(std::string animation_state)
    {
        animation_state_ = std::move(animation_state);
    }

    [[nodiscard]] const std::string& GetAnimationState() const
    {
        return animation_state_;
    }

    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "InteractivePropActor";
    }

private:
    std::string archetype_id_ = "interactive_prop";
    std::string animation_state_ = "idle";
};
} // namespace grpcmmo::client
