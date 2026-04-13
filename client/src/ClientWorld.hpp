#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "ActorFactory.hpp"
#include "Camera.hpp"
#include "CameraDirector.hpp"
#include "FrameSceneBridge.hpp"
#include "MoveCommand.hpp"
#include "PlayerController.hpp"
#include "WorldActor.hpp"
#include "session/v1/session.pb.h"
#include "world/v1/replication.pb.h"

namespace frame
{
class WindowInterface;
namespace proto
{
class Level;
}
} // namespace frame

namespace grpcmmo::client
{
struct ClientWorldConfig
{
    std::chrono::duration<float> auto_move_duration{0.0f};
    bool debug_pose_trace = false;
};

class ClientWorld
{
public:
    void Configure(const ClientWorldConfig& config);
    [[nodiscard]] frame::proto::Level BuildLevelProto() const;

    void Attach(frame::WindowInterface& window);
    void Init();
    void End();

    void OnSessionReady(const grpcmmo::session::v1::SessionReady& session_ready);
    void ApplyReplicationBatch(const grpcmmo::world::v1::ReplicationBatch& batch);
    void Tick(float delta_seconds);

    [[nodiscard]] bool IsExitRequested() const;
    [[nodiscard]] bool HasControlledPawn() const;
    [[nodiscard]] bool HasPendingMoveCommand() const;
    [[nodiscard]] const MoveCommand& GetPendingMoveCommand() const;
    void OnMoveSent();

private:
    using ReplicatedActorMap = std::unordered_map<std::string, std::unique_ptr<Actor>>;

    [[nodiscard]] PlayerController::FrameInput BuildFrameInput() const;
    void BootstrapLocalWorld();
    void ClearLocalWorldActors();
    void ClearReplicatedActors();
    void ApplyEntityPatch(const grpcmmo::world::v1::EntityPatch& patch);
    void RemoveEntity(const std::string& entity_id);
    [[nodiscard]] Pawn* GetControlledPawn();
    [[nodiscard]] const Pawn* GetControlledPawn() const;

    ClientWorldConfig config_{};
    frame::WindowInterface* window_ = nullptr;
    PlayerController* player_controller_ = nullptr;
    ActorFactory actor_factory_{};
    FrameSceneBridge scene_bridge_{};
    CameraDirector camera_director_{};
    Camera viewport_camera_{};
    CameraActor* gameplay_camera_ = nullptr;
    std::vector<std::unique_ptr<PlanetActor>> planets_{};
    std::vector<std::unique_ptr<StaticPropActor>> static_props_{};
    std::vector<std::unique_ptr<InteractivePropActor>> interactive_props_{};
    ReplicatedActorMap replicated_actors_{};
    std::chrono::steady_clock::time_point session_ready_at_{};
    std::string controlled_entity_id_{};
    MoveCommand pending_move_command_{};
    glm::vec3 last_sent_facing_direction_{1.0f, 0.0f, 0.0f};
    bool last_sent_facing_direction_valid_ = false;
    bool exit_requested_ = false;
    bool session_ready_received_ = false;
};
} // namespace grpcmmo::client
