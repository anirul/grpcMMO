#include "ClientWorld.hpp"

#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "Character.hpp"
#include "frame/json/proto.h"
#include "frame/window_interface.h"
#include "grpcmmo/shared/planet/PreviewPatchConfig.hpp"

namespace grpcmmo::client
{
namespace
{
using grpcmmo::shared::planet::BuildPreviewPatchFrame;
using grpcmmo::shared::planet::BuildPreviewPatchOriginPlanetPosition;
using grpcmmo::shared::planet::BuildPreviewPatchUp;
using grpcmmo::shared::planet::kMarsPreviewPatch000;
using grpcmmo::shared::planet::LocalDirectionToWorld;
using grpcmmo::shared::planet::NormalizeOrFallback;
using grpcmmo::shared::planet::ProjectDirectionOntoTangent;
using grpcmmo::shared::planet::ProjectVectorOntoTangent;
using grpcmmo::shared::planet::SurfaceUpFromPosition;
using grpcmmo::shared::planet::WorldDirectionToLocal;
using grpcmmo::shared::planet::WorldPositionToLocal;

bool NearlyEqualDirection(const glm::vec3& lhs, const glm::vec3& rhs)
{
    const float lhs_length_squared = glm::dot(lhs, lhs);
    const float rhs_length_squared = glm::dot(rhs, rhs);
    if (lhs_length_squared <= 0.000001f || rhs_length_squared <= 0.000001f)
    {
        return lhs_length_squared <= 0.000001f &&
               rhs_length_squared <= 0.000001f;
    }

    const float alignment = glm::dot(
        lhs / std::sqrt(lhs_length_squared),
        rhs / std::sqrt(rhs_length_squared));
    return alignment >= 0.99995f;
}

constexpr double kHardWorldCorrectionMeters = 1.25;
constexpr double kSoftWorldCorrectionMeters = 0.35;
constexpr double kIdleWorldCorrectionRate = 7.0;

[[nodiscard]] const grpcmmo::shared::planet::PreviewPatchConfig& PreviewPatch()
{
    return kMarsPreviewPatch000;
}

[[nodiscard]] const grpcmmo::shared::planet::TangentFrame& PreviewPatchFrame()
{
    static const grpcmmo::shared::planet::TangentFrame frame =
        BuildPreviewPatchFrame(PreviewPatch());
    return frame;
}

[[nodiscard]] const glm::dvec3& PreviewPatchOriginPlanetPosition()
{
    static const glm::dvec3 origin_position =
        BuildPreviewPatchOriginPlanetPosition(PreviewPatch());
    return origin_position;
}

[[nodiscard]] glm::dvec3 ToDVec3(const grpcmmo::world::v1::Vector3d& value)
{
    return glm::dvec3(value.x(), value.y(), value.z());
}

[[nodiscard]] glm::dquat ToDQuat(const grpcmmo::world::v1::Quaterniond& value)
{
    return glm::normalize(
        glm::dquat(value.w(), value.x(), value.y(), value.z()));
}

[[nodiscard]] glm::vec3 ToVec3(const glm::dvec3& value)
{
    return glm::vec3(
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z));
}

[[nodiscard]] glm::vec3 NormalizeOrFallbackFloat(
    const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    if (length_squared <= 0.000001f)
    {
        return fallback;
    }
    return value / std::sqrt(length_squared);
}

[[nodiscard]] glm::vec3 ToRenderPosition(
    const grpcmmo::world::v1::EntityPatch& patch)
{
    return ToVec3(WorldPositionToLocal(
        ToDVec3(patch.transform().position_m()),
        PreviewPatchOriginPlanetPosition(),
        PreviewPatchFrame()));
}

[[nodiscard]] glm::vec3 ToRenderPosition(const glm::dvec3& world_position)
{
    return ToVec3(WorldPositionToLocal(
        world_position,
        PreviewPatchOriginPlanetPosition(),
        PreviewPatchFrame()));
}

[[nodiscard]] glm::vec3 ToRenderSurfaceUp(
    const grpcmmo::world::v1::EntityPatch& patch)
{
    const glm::dvec3 world_up = NormalizeOrFallback(
        patch.transform().has_up_unit()
            ? ToDVec3(patch.transform().up_unit())
            : SurfaceUpFromPosition(ToDVec3(patch.transform().position_m())),
        BuildPreviewPatchUp(PreviewPatch()));
    return NormalizeOrFallbackFloat(
        ToVec3(WorldDirectionToLocal(world_up, PreviewPatchFrame())),
        glm::vec3(0.0f, 1.0f, 0.0f));
}

[[nodiscard]] glm::vec3 ToRenderSurfaceUp(const glm::dvec3& world_position)
{
    const glm::dvec3 world_up = NormalizeOrFallback(
        SurfaceUpFromPosition(world_position),
        BuildPreviewPatchUp(PreviewPatch()));
    return NormalizeOrFallbackFloat(
        ToVec3(WorldDirectionToLocal(world_up, PreviewPatchFrame())),
        glm::vec3(0.0f, 1.0f, 0.0f));
}

[[nodiscard]] glm::vec3 ToFacingDirection(
    const grpcmmo::world::v1::EntityPatch& patch)
{
    const glm::dvec3 world_forward = glm::rotate(
        ToDQuat(patch.transform().orientation()), glm::dvec3(1.0, 0.0, 0.0));
    const glm::vec3 local_up = ToRenderSurfaceUp(patch);
    return NormalizeOrFallbackFloat(
        ToVec3(ProjectDirectionOntoTangent(
            WorldDirectionToLocal(world_forward, PreviewPatchFrame()),
            glm::dvec3(local_up.x, local_up.y, local_up.z),
            glm::dvec3(1.0, 0.0, 0.0))),
        glm::vec3(1.0f, 0.0f, 0.0f));
}

[[nodiscard]] glm::quat ToRenderOrientation(
    const grpcmmo::world::v1::EntityPatch& patch)
{
    const glm::vec3 forward = ToFacingDirection(patch);
    const glm::vec3 up = ToRenderSurfaceUp(patch);
    const glm::vec3 side = NormalizeOrFallbackFloat(
        glm::cross(forward, up), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 corrected_up =
        NormalizeOrFallbackFloat(glm::cross(side, forward), up);
    return glm::normalize(
        glm::quat_cast(glm::mat3(forward, corrected_up, side)));
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
    controlled_authoritative_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_predicted_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_world_position_valid_ = false;
    last_sent_facing_direction_valid_ = false;

    ClearReplicatedActors();
    ClearLocalWorldActors();
    BootstrapLocalWorld();
    (void)terrain_sampler_.LoadPreviewPatch();

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
    controlled_authoritative_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_predicted_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_world_position_valid_ = false;
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
    MoveCommand local_move_command;

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

    if (Pawn* controlled_pawn = GetControlledPawn();
        controlled_pawn != nullptr && player_controller_ != nullptr)
    {
        const glm::vec3 look_facing_direction =
            player_controller_->GetLookFacingDirection();
        controlled_pawn->SetLocalFacingDirection(look_facing_direction);
        if (!last_sent_facing_direction_valid_ ||
            !NearlyEqualDirection(
                look_facing_direction, last_sent_facing_direction_))
        {
            MoveCommand facing_command;
            facing_command.SetFacingDirection(
                glm::dvec3(look_facing_direction));
            pending_move_command_.Accumulate(facing_command);
        }

        local_move_command =
            player_controller_->DrivePawn(frame_input, delta_seconds);
        controlled_pawn->NotifyLocalInput(local_move_command);
        AdvanceControlledPrediction(local_move_command);
        pending_move_command_.Accumulate(local_move_command);
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

    if (controlled_world_position_valid_)
    {
        const double world_error_m = glm::length(
            controlled_authoritative_world_position_ -
            controlled_predicted_world_position_);
        if (world_error_m > kHardWorldCorrectionMeters)
        {
            controlled_predicted_world_position_ =
                controlled_authoritative_world_position_;
        }
        else if (
            world_error_m > kSoftWorldCorrectionMeters &&
            !local_move_command.HasTranslation())
        {
            const double alpha = std::clamp(
                static_cast<double>(delta_seconds) * kIdleWorldCorrectionRate,
                0.0,
                1.0);
            controlled_predicted_world_position_ = glm::mix(
                controlled_predicted_world_position_,
                controlled_authoritative_world_position_,
                alpha);
        }
    }

    SyncControlledPawnRenderState();

    const CameraBoon* active_boon = player_controller_ != nullptr
                                        ? &player_controller_->GetCameraBoon()
                                        : nullptr;
    scene_bridge_.SetViewState(GetControlledPawn(), active_boon);
    scene_bridge_.Tick(delta_seconds);

    if (gameplay_camera_ != nullptr && active_boon != nullptr)
    {
        gameplay_camera_->SetPose(scene_bridge_.BuildFollowCameraPose(
            GetControlledPawn(), *active_boon));
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
        last_sent_facing_direction_ =
            pending_move_command_.facing_direction_unit;
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

    const float elapsed_seconds =
        std::chrono::duration<float>(
            std::chrono::steady_clock::now() - session_ready_at_)
            .count();
    const bool auto_move_active =
        config_.auto_move_duration.count() > 0.0f &&
        elapsed_seconds <= config_.auto_move_duration.count();
    if (auto_move_active)
    {
        frame_input.move_forward = 1.0f;
    }
    return frame_input;
}

glm::vec3 ClientWorld::GroundRenderPosition(
    const glm::vec3& render_position) const
{
    return terrain_sampler_.GroundLocalPosition(render_position);
}

void ClientWorld::AdvanceControlledPrediction(const MoveCommand& move_command)
{
    if (!controlled_world_position_valid_ || !move_command.HasTranslation())
    {
        return;
    }

    const glm::dvec3 local_step = move_command.world_displacement_m;
    const glm::dvec3 current_up =
        SurfaceUpFromPosition(controlled_predicted_world_position_);
    const glm::dvec3 requested_world_step = ProjectVectorOntoTangent(
        LocalDirectionToWorld(local_step, PreviewPatchFrame()), current_up);
    controlled_predicted_world_position_ = terrain_sampler_.GroundWorldPosition(
        controlled_predicted_world_position_ + requested_world_step);
}

void ClientWorld::SyncControlledPawnRenderState()
{
    Pawn* controlled_pawn = GetControlledPawn();
    if (controlled_pawn == nullptr || !controlled_world_position_valid_)
    {
        return;
    }

    controlled_pawn->SetPredictedRenderState(
        ToRenderPosition(controlled_predicted_world_position_),
        ToRenderSurfaceUp(controlled_predicted_world_position_));
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
    controlled_authoritative_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_predicted_world_position_ = glm::dvec3(0.0, 0.0, 0.0);
    controlled_world_position_valid_ = false;
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
        const glm::dvec3 grounded_world_position =
            terrain_sampler_.GroundWorldPosition(
                ToDVec3(patch.transform().position_m()));
        PawnSnapshot snapshot;
        snapshot.pawn_id = patch.entity_id();
        snapshot.display_name = patch.metadata().display_name();
        snapshot.position = ToRenderPosition(grounded_world_position);
        snapshot.facing_direction = ToFacingDirection(patch);
        snapshot.surface_up = ToRenderSurfaceUp(grounded_world_position);
        snapshot.controlled = patch.metadata().controlled_entity();
        pawn->ApplyReplication(snapshot);

        if (patch.metadata().controlled_entity())
        {
            controlled_authoritative_world_position_ = grounded_world_position;
            if (!controlled_world_position_valid_ ||
                controlled_entity_id_ != patch.entity_id())
            {
                controlled_predicted_world_position_ = grounded_world_position;
                controlled_world_position_valid_ = true;
            }
            SyncControlledPawnRenderState();
        }
    }
    else
    {
        actor->GetRootComponent().SetWorldPosition(ToRenderPosition(patch));
        actor->GetRootComponent().SetWorldOrientation(
            ToRenderOrientation(patch));
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
