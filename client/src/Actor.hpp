#pragma once

#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Object.hpp"

namespace grpcmmo::client
{
class Actor;

class ActorComponent : public Object
{
public:
    void Init() override
    {
    }

    void End() override
    {
    }

    void Tick(float /*delta_seconds*/) override
    {
    }

    void SetOwner(Actor* owner)
    {
        owner_ = owner;
    }

    [[nodiscard]] Actor* GetOwner()
    {
        return owner_;
    }

    [[nodiscard]] const Actor* GetOwner() const
    {
        return owner_;
    }

private:
    Actor* owner_ = nullptr;
};

class SceneComponent : public ActorComponent
{
public:
    void SetWorldPosition(const glm::vec3& world_position)
    {
        world_position_ = world_position;
    }

    void SetWorldOrientation(const glm::quat& world_orientation)
    {
        world_orientation_ = glm::normalize(world_orientation);
    }

    void SetWorldScale(const glm::vec3& world_scale)
    {
        world_scale_ = world_scale;
    }

    [[nodiscard]] const glm::vec3& GetWorldPosition() const
    {
        return world_position_;
    }

    [[nodiscard]] const glm::quat& GetWorldOrientation() const
    {
        return world_orientation_;
    }

    [[nodiscard]] const glm::vec3& GetWorldScale() const
    {
        return world_scale_;
    }

    [[nodiscard]] glm::vec3 GetForwardVector() const
    {
        return glm::rotate(world_orientation_, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    [[nodiscard]] glm::mat4 BuildWorldMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), world_position_) *
               glm::toMat4(world_orientation_) *
               glm::scale(glm::mat4(1.0f), world_scale_);
    }

private:
    glm::vec3 world_position_ = glm::vec3(0.0f);
    glm::quat world_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 world_scale_ = glm::vec3(1.0f);
};

class Actor : public Object
{
public:
    void Init() override
    {
        root_component_.SetOwner(this);
        root_component_.Init();
    }

    void End() override
    {
        root_component_.End();
        root_component_.SetOwner(nullptr);
    }

    void Tick(float delta_seconds) override
    {
        root_component_.Tick(delta_seconds);
    }

    void SetEntityId(std::string entity_id)
    {
        entity_id_ = std::move(entity_id);
    }

    [[nodiscard]] const std::string& GetEntityId() const
    {
        return entity_id_;
    }

    void SetDisplayName(std::string display_name)
    {
        display_name_ = std::move(display_name);
    }

    [[nodiscard]] const std::string& GetDisplayName() const
    {
        return display_name_;
    }

    void SetReplicated(bool replicated)
    {
        replicated_ = replicated;
    }

    [[nodiscard]] bool IsReplicated() const
    {
        return replicated_;
    }

    [[nodiscard]] SceneComponent& GetRootComponent()
    {
        return root_component_;
    }

    [[nodiscard]] const SceneComponent& GetRootComponent() const
    {
        return root_component_;
    }

    [[nodiscard]] virtual const char* GetActorClassName() const
    {
        return "Actor";
    }

private:
    std::string entity_id_;
    std::string display_name_;
    bool replicated_ = false;
    SceneComponent root_component_{};
};
} // namespace grpcmmo::client
