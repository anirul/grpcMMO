#include "ClientWorld.hpp"

#include <cmath>

#include "Character.hpp"
#include "frame/json/proto.h"
#include "frame/window_interface.h"

namespace grpcmmo::client
{
namespace
{
bool NearlyEqualDirection(const glm::vec3& lhs, const glm::vec3& rhs)
{
    const float lhs_length_squared = glm::dot(lhs, lhs);
    const float rhs_length_squared = glm::dot(rhs, rhs);
    if (lhs_length_squared <= 0.000001f || rhs_length_squared <= 0.000001f)
    {
        return lhs_length_squared <= 0.000001f && rhs_length_squared <= 0.000001f;
    }

    const float alignment =
        glm::dot(lhs / std::sqrt(lhs_length_squared), rhs / std::sqrt(rhs_length_squared));
    return alignment >= 0.99995f;
}

glm::vec3 ToRenderPosition(const grpcmmo::world::v1::EntityPatch& patch)
{
    return glm::vec3(static_cast<float>(patch.transform().position_m().x()),
                     0.0f,
                     static_cast<float>(patch.transform().position_m().z()));
}

glm::quat ToRenderOrientation(const grpcmmo::world::v1::EntityPatch& patch)
{
    const auto& orientation = patch.transform().orientation();
    return glm::normalize(glm::quat(static_cast<float>(orientation.w()),
                                    static_cast<float>(orientation.x()),
                                    static_cast<float>(orientation.y()),
                                    static_cast<float>(orientation.z())));
}

glm::vec3 ToFacingDirection(const grpcmmo::world::v1::EntityPatch& patch)
{
    glm::vec3 facing_direction =
        glm::rotate(ToRenderOrientation(patch), glm::vec3(1.0f, 0.0f, 0.0f));
    facing_direction.y = 0.0f;
    const float length_squared = glm::dot(facing_direction, facing_direction);
    if (length_squared <= 0.000001f)
    {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }

    return facing_direction / std::sqrt(length_squared);
}
} // namespace

void ClientWorld::Configure(const ClientWorldConfig& config)
{
    config_ = config;
    scene_bridge_.SetDebugPoseTrace(config_.debug_pose_trace);
}

frame::proto::Level ClientWorld::BuildLevelProto() const
{
    return scene_bridge_.BuildLevelProto();
}

void ClientWorld::Attach(frame::WindowInterface& window)
{
    window_ = &window;
    scene_bridge_.Attach(&window);
    viewport_camera_.Attach(&window);

    auto player_controller = std::make_unique<PlayerController>();
    player_controller_ = player_controller.get();
    window.SetInputInterface(std::move(player_controller));
}

void ClientWorld::Init()
{
    exit_requested_ = false;
    session_ready_received_ = false;
    session_ready_at_ = {};
    controlled_entity_id_.clear();
    pending_move_command_.Clear();
    last_sent_facing_direction_ = glm::vec3(1.0f, 0.0f, 0.0f);
    last_sent_facing_direction_valid_ = false;

    ClearReplicatedActors();
    ClearLocalWorldActors();
    BootstrapLocalWorld();

    if (player_controller_ != nullptr)
    {
        player_controller_->Init();
    }

    camera_director_.Init();
    gameplay_camera_ = &camera_director_.CreateCamera("gameplay_follow_camera");
    const bool activated =
        camera_director_.SetActiveCamera(gameplay_camera_->GetCameraId());
    (void)activated;

    scene_bridge_.SetDebugPoseTrace(config_.debug_pose_trace);
    scene_bridge_.Init();
    if (window_ != nullptr)
    {
        scene_bridge_.Attach(window_);
    }
    viewport_camera_.Init();
    if (window_ != nullptr)
    {
        viewport_camera_.Attach(window_);
    }
}

void ClientWorld::End()
{
    if (player_controller_ != nullptr)
    {
        player_controller_->End();
        player_controller_ = nullptr;
    }

    viewport_camera_.End();
    scene_bridge_.End();
    camera_director_.End();
    gameplay_camera_ = nullptr;

    ClearReplicatedActors();
    ClearLocalWorldActors();
    window_ = nullptr;
}

void ClientWorld::OnSessionReady(
    const grpcmmo::session::v1::SessionReady& session_ready)
{
    session_ready_received_ = true;
    session_ready_at_ = std::chrono::steady_clock::now();
    controlled_entity_id_ = session_ready.controlled_entity_id();
    ApplyEntityPatch(session_ready.initial_controlled_entity());
}

void ClientWorld::ApplyReplicationBatch(
    const grpcmmo::world::v1::ReplicationBatch& batch)
{
    for (const auto& patch : batch.entities())
    {
        ApplyEntityPatch(patch);
    }

    for (const auto& removed_entity_id : batch.removed_entity_ids())
    {
        RemoveEntity(removed_entity_id);
    }
}

void ClientWorld::Tick(float delta_seconds)
{
    if (player_controller_ != nullptr)
    {
        player_controller_->Tick(delta_seconds);
    }

    const PlayerController::FrameInput frame_input = BuildFrameInput();
    if (frame_input.exit_requested)
    {
        exit_requested_ = true;
    }

    if (player_controller_ != nullptr)
    {
        player_controller_->DriveCamera(frame_input);
    }

    if (Pawn* controlled_pawn = GetControlledPawn(); controlled_pawn != nullptr &&
        player_controller_ != nullptr)
    {
        const glm::vec3 look_facing_direction = player_controller_->GetLookFacingDirection();
        controlled_pawn->SetLocalFacingDirection(look_facing_direction);
        if (!last_sent_facing_direction_valid_ ||
            !NearlyEqualDirection(look_facing_direction, last_sent_facing_direction_))
        {
            MoveCommand facing_command;
            facing_command.SetFacingDirection(look_facing_direction);
            pending_move_command_.Accumulate(facing_command);
        }

        pending_move_command_.Accumulate(
            player_controller_->DrivePawn(frame_input, delta_seconds));
    }

    for (auto& planet : planets_)
    {
        planet->Tick(delta_seconds);
    }
    for (auto& prop : static_props_)
    {
        prop->Tick(delta_seconds);
    }
    for (auto& prop : interactive_props_)
    {
        prop->Tick(delta_seconds);
    }
    for (auto& [entity_id, actor] : replicated_actors_)
    {
        actor->Tick(delta_seconds);
    }

    const CameraBoon* active_boon =
        player_controller_ != nullptr ? &player_controller_->GetCameraBoon() : nullptr;
    scene_bridge_.SetViewState(GetControlledPawn(), active_boon);
    scene_bridge_.Tick(delta_seconds);

    if (gameplay_camera_ != nullptr && active_boon != nullptr)
    {
        gameplay_camera_->SetPose(
            scene_bridge_.BuildFollowCameraPose(GetControlledPawn(), *active_boon));
    }

    camera_director_.Tick(delta_seconds);
    CameraPose active_pose;
    if (camera_director_.TryGetActiveCameraPose(&active_pose))
    {
        viewport_camera_.SetPose(active_pose);
    }
    viewport_camera_.Tick(delta_seconds);
}

bool ClientWorld::IsExitRequested() const
{
    return exit_requested_;
}

bool ClientWorld::HasControlledPawn() const
{
    return GetControlledPawn() != nullptr;
}

bool ClientWorld::HasPendingMoveCommand() const
{
    return pending_move_command_.HasSignal();
}

const MoveCommand& ClientWorld::GetPendingMoveCommand() const
{
    return pending_move_command_;
}

void ClientWorld::OnMoveSent()
{
    if (pending_move_command_.has_facing_direction)
    {
        last_sent_facing_direction_ = pending_move_command_.facing_direction_unit;
        last_sent_facing_direction_valid_ = true;
    }
    pending_move_command_.Clear();
}

PlayerController::FrameInput ClientWorld::BuildFrameInput() const
{
    PlayerController::FrameInput frame_input =
        player_controller_ != nullptr ? player_controller_->ConsumeFrameInput()
                                      : PlayerController::FrameInput{};

    if (!session_ready_received_)
    {
        return frame_input;
    }

    const float elapsed_seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - session_ready_at_).count();
    const bool auto_move_active =
        config_.auto_move_duration.count() > 0.0f &&
        elapsed_seconds <= config_.auto_move_duration.count();
    if (auto_move_active)
    {
        frame_input.move_forward = 1.0f;
    }
    return frame_input;
}

void ClientWorld::BootstrapLocalWorld()
{
    auto planet = actor_factory_.CreatePlanetActor("planet-root", "Planet");
    planet->Init();
    planets_.push_back(std::move(planet));
}

void ClientWorld::ClearLocalWorldActors()
{
    for (auto& planet : planets_)
    {
        planet->End();
    }
    planets_.clear();

    for (auto& prop : static_props_)
    {
        prop->End();
    }
    static_props_.clear();

    for (auto& prop : interactive_props_)
    {
        prop->End();
    }
    interactive_props_.clear();
}

void ClientWorld::ClearReplicatedActors()
{
    if (player_controller_ != nullptr)
    {
        player_controller_->UnPossess();
    }

    for (auto& [entity_id, actor] : replicated_actors_)
    {
        actor->End();
    }
    replicated_actors_.clear();
    controlled_entity_id_.clear();
}

void ClientWorld::ApplyEntityPatch(const grpcmmo::world::v1::EntityPatch& patch)
{
    const auto it = replicated_actors_.find(patch.entity_id());
    Actor* actor = nullptr;
    if (it == replicated_actors_.end())
    {
        auto created_actor = actor_factory_.CreateReplicatedActor(patch);
        created_actor->Init();
        actor = created_actor.get();
        replicated_actors_.emplace(patch.entity_id(), std::move(created_actor));
    }
    else
    {
        actor = it->second.get();
    }

    actor->SetEntityId(patch.entity_id());
    actor->SetDisplayName(patch.metadata().display_name());
    actor->SetReplicated(true);

    if (Pawn* pawn = dynamic_cast<Pawn*>(actor); pawn != nullptr)
    {
        PawnSnapshot snapshot;
        snapshot.pawn_id = patch.entity_id();
        snapshot.display_name = patch.metadata().display_name();
        snapshot.position = ToRenderPosition(patch);
        snapshot.facing_direction = ToFacingDirection(patch);
        snapshot.controlled = patch.metadata().controlled_entity();
        pawn->ApplyReplication(snapshot);
    }
    else
    {
        actor->GetRootComponent().SetWorldPosition(ToRenderPosition(patch));
        actor->GetRootComponent().SetWorldOrientation(ToRenderOrientation(patch));
        actor->GetRootComponent().SetWorldScale(glm::vec3(1.0f));
    }

    if (patch.metadata().controlled_entity())
    {
        controlled_entity_id_ = patch.entity_id();
        if (player_controller_ != nullptr)
        {
            player_controller_->Possess(dynamic_cast<Pawn*>(actor));
        }
    }
}

void ClientWorld::RemoveEntity(const std::string& entity_id)
{
    const auto it = replicated_actors_.find(entity_id);
    if (it == replicated_actors_.end())
    {
        return;
    }

    if (entity_id == controlled_entity_id_)
    {
        controlled_entity_id_.clear();
        if (player_controller_ != nullptr)
        {
            player_controller_->UnPossess();
        }
    }

    it->second->End();
    replicated_actors_.erase(it);
}

Pawn* ClientWorld::GetControlledPawn()
{
    if (controlled_entity_id_.empty())
    {
        return nullptr;
    }

    const auto it = replicated_actors_.find(controlled_entity_id_);
    if (it == replicated_actors_.end())
    {
        return nullptr;
    }

    return dynamic_cast<Pawn*>(it->second.get());
}

const Pawn* ClientWorld::GetControlledPawn() const
{
    if (controlled_entity_id_.empty())
    {
        return nullptr;
    }

    const auto it = replicated_actors_.find(controlled_entity_id_);
    if (it == replicated_actors_.end())
    {
        return nullptr;
    }

    return dynamic_cast<const Pawn*>(it->second.get());
}
} // namespace grpcmmo::client
