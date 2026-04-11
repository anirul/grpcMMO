#pragma once

#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "MoveCommand.hpp"
#include "Object.hpp"

namespace grpcmmo::client
{
struct PawnSnapshot
{
    std::string pawn_id;
    std::string display_name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool controlled = false;
};

class Pawn : public Object
{
public:
    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    void ApplyReplication(const PawnSnapshot& snapshot);
    void ApplyMove(const MoveCommand& move_command);
    void SetLocalFacingOrientation(const glm::quat& orientation);

    [[nodiscard]] const std::string& GetEntityId() const;
    [[nodiscard]] glm::vec3 GetRenderPosition() const;
    [[nodiscard]] glm::quat GetRenderOrientation() const;
    [[nodiscard]] float GetRenderYawRadians() const;
    [[nodiscard]] bool IsControlled() const;
    [[nodiscard]] glm::vec3 GetSurfaceUp() const;

    static float ExtractYawRadians(const glm::quat& orientation);

private:
    void Reconcile(float delta_seconds);

    std::string entity_id_;
    std::string display_name_;
    glm::vec3 authoritative_position_ = glm::vec3(0.0f);
    glm::quat authoritative_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 predicted_position_ = glm::vec3(0.0f);
    glm::quat predicted_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    float seconds_since_local_input_ = 1000.0f;
    bool controlled_ = false;
    bool initialized_ = false;
};
} // namespace grpcmmo::client
