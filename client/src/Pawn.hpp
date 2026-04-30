#pragma once

#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Actor.hpp"
#include "MoveCommand.hpp"

namespace grpcmmo::client
{
    struct PawnSnapshot
    {
        std::string pawn_id;
        std::string display_name;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 facing_direction = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 surface_up = glm::vec3(0.0f, 1.0f, 0.0f);
        bool controlled = false;
    };

    class Pawn : public Actor
    {
      public:
        void Init() override;
        void End() override;
        void Tick(float delta_seconds) override;

        void ApplyReplication(const PawnSnapshot& snapshot);
        void ApplyMove(const MoveCommand& move_command);
        void NotifyLocalInput(const MoveCommand& move_command);
        void SetLocalFacingDirection(const glm::vec3& direction);
        void SetPredictedPosition(const glm::vec3& position);
        void SetPredictedSurfaceUp(const glm::vec3& surface_up);
        void SetPredictedRenderState(
            const glm::vec3& position, const glm::vec3& surface_up
        );

        [[nodiscard]] const std::string& GetEntityId() const;
        [[nodiscard]] glm::vec3 GetRenderFacingDirection() const;
        [[nodiscard]] glm::vec3 GetRenderPosition() const;
        [[nodiscard]] glm::quat GetRenderOrientation() const;
        [[nodiscard]] float GetRenderYawRadians() const;
        [[nodiscard]] bool IsControlled() const;
        [[nodiscard]] glm::vec3 GetSurfaceUp() const;
        [[nodiscard]] const char* GetActorClassName() const override;

      private:
        void Reconcile(float delta_seconds);
        void SyncRootComponentFromRenderState();

        glm::vec3 authoritative_position_ = glm::vec3(0.0f);
        glm::vec3 authoritative_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 authoritative_surface_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 predicted_position_ = glm::vec3(0.0f);
        glm::vec3 predicted_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 predicted_surface_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        float seconds_since_local_input_ = 1000.0f;
        bool controlled_ = false;
        bool initialized_ = false;
    };
} // namespace grpcmmo::client
