#include "grpcmmo/game/AuthoritativeWorld.hpp"

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "grpcmmo/shared/planet/PlanetMath.hpp"
#include "grpcmmo/shared/Time.hpp"

namespace grpcmmo::game
{
namespace
{
using grpcmmo::shared::planet::AltitudeFromPosition;
using grpcmmo::shared::planet::BuildPreviewPatchFrame;
using grpcmmo::shared::planet::LocalDirectionToWorld;
using grpcmmo::shared::planet::NormalizeOrFallback;
using grpcmmo::shared::planet::PreviewPatchSurfaceAltitudeM;
using grpcmmo::shared::planet::ProjectVectorOntoTangent;
using grpcmmo::shared::planet::ProjectDirectionOntoTangent;
using grpcmmo::shared::planet::ProjectToAltitude;
using grpcmmo::shared::planet::SurfaceUpFromPosition;
using grpcmmo::shared::planet::kMarsPreviewPatch000;

constexpr double kMoveSpeedMetersPerSecond = 4.0;
constexpr double kDefaultInputStepSeconds = 0.05;
constexpr double kMinInputStepSeconds = 1.0 / 120.0;
constexpr double kMaxInputStepSeconds = 0.10;
constexpr double kSpawnSpacingMeters = 6.0;

[[nodiscard]] const grpcmmo::shared::planet::PreviewPatchConfig&
PreviewPatch()
{
    return kMarsPreviewPatch000;
}

[[nodiscard]] const grpcmmo::shared::planet::TangentFrame&
PreviewPatchFrame()
{
    static const grpcmmo::shared::planet::TangentFrame frame =
        BuildPreviewPatchFrame(PreviewPatch());
    return frame;
}

[[nodiscard]] glm::dvec3 NormalizeOrXAxis(const glm::dvec3& value)
{
    return NormalizeOrFallback(value, glm::dvec3(1.0, 0.0, 0.0));
}

[[nodiscard]] glm::dvec3 LocalVectorToWorldTangent(const glm::dvec3& local_vector,
                                                   const glm::dvec3& up_unit)
{
    return ProjectVectorOntoTangent(LocalDirectionToWorld(local_vector, PreviewPatchFrame()),
                                    up_unit);
}

[[nodiscard]] glm::dvec3 LocalDirectionToWorldTangent(const glm::dvec3& local_direction,
                                                      const glm::dvec3& up_unit,
                                                      const glm::dvec3& fallback)
{
    return ProjectDirectionOntoTangent(LocalDirectionToWorld(local_direction, PreviewPatchFrame()),
                                       up_unit,
                                       fallback);
}

template <typename QuaternionT>
void SetOrientationFromForwardUp(QuaternionT* orientation,
                                 const glm::dvec3& forward_direction,
                                 const glm::dvec3& up_direction)
{
    const glm::dvec3 up = NormalizeOrFallback(up_direction, glm::dvec3(0.0, 1.0, 0.0));
    const glm::dvec3 forward =
        ProjectDirectionOntoTangent(forward_direction, up, glm::dvec3(1.0, 0.0, 0.0));
    const glm::dvec3 side =
        NormalizeOrFallback(glm::cross(forward, up), glm::dvec3(0.0, 0.0, 1.0));
    const glm::dvec3 corrected_up =
        NormalizeOrFallback(glm::cross(side, forward), up);
    const glm::dmat3 basis(forward, corrected_up, side);
    const glm::dquat rotation = glm::normalize(glm::quat_cast(basis));

    orientation->set_x(rotation.x);
    orientation->set_y(rotation.y);
    orientation->set_z(rotation.z);
    orientation->set_w(rotation.w);
}

[[nodiscard]] glm::dvec3 BuildSpawnPosition(std::size_t spawn_index,
                                            double planet_radius_m,
                                            const grpcmmo::shared::planet::PreviewPatchTerrainSampler& terrain_sampler)
{
    const double spawn_offset_m =
        static_cast<double>(spawn_index) * kSpawnSpacingMeters;
    const glm::dvec3 approximate_position =
        grpcmmo::shared::planet::BuildPreviewPatchOriginPlanetPosition(PreviewPatch()) +
        (PreviewPatchFrame().east * spawn_offset_m);
    if (terrain_sampler.IsLoaded())
    {
        return terrain_sampler.GroundWorldPosition(approximate_position, planet_radius_m);
    }

    return ProjectToAltitude(approximate_position,
                             planet_radius_m,
                             PreviewPatchSurfaceAltitudeM(PreviewPatch()));
}

[[nodiscard]] glm::dvec3 BuildSpawnFacingDirection(
    const glm::dvec3& planet_position_m)
{
    const glm::dvec3 up = SurfaceUpFromPosition(planet_position_m);
    return ProjectDirectionOntoTangent(PreviewPatchFrame().east,
                                       up,
                                       glm::dvec3(1.0, 0.0, 0.0));
}

[[nodiscard]] bool NearlyChanged(const glm::dvec3& lhs, const glm::dvec3& rhs)
{
    return glm::length(lhs - rhs) > 0.0001;
}

[[nodiscard]] glm::dvec3 ClampLocalStep(const glm::dvec3& requested_step_m,
                                        double max_distance_m)
{
    const double requested_distance = glm::length(requested_step_m);
    if (requested_distance <= max_distance_m || requested_distance <= 0.000001)
    {
        return requested_step_m;
    }

    return requested_step_m * (max_distance_m / requested_distance);
}
} // namespace

AuthoritativeWorld::AuthoritativeWorld(double planet_radius_m)
    : planet_radius_m_(planet_radius_m)
{
    (void)terrain_sampler_.Load(PreviewPatch());
}

AuthoritativeWorld::ConnectResult AuthoritativeWorld::ConnectPlayer(
    const ConnectedPlayer& player)
{
    std::scoped_lock lock(mutex_);

    const std::size_t spawn_index = players_by_session_.size();
    PlayerState player_state;
    player_state.session_id = player.session_id;
    player_state.character_id = player.character_id;
    player_state.character_name = player.character_name;
    player_state.entity_id = "entity-" + player.character_id;
    player_state.planet_id = player.planet_id;
    player_state.zone_id = player.zone_id;
    player_state.patch_id = player.patch_id.empty() ? "patch-000" : player.patch_id;
    player_state.position_m = BuildSpawnPosition(spawn_index,
                                                planet_radius_m_,
                                                terrain_sampler_);
    player_state.facing_direction_unit =
        BuildSpawnFacingDirection(player_state.position_m);

    const std::uint64_t server_time_ms = grpcmmo::shared::NowMs();
    const std::uint64_t server_tick = next_tick_++;

    players_by_session_[player_state.session_id] = player_state;
    auto& stored_state = players_by_session_.at(player_state.session_id);
    stored_state.last_sent_time_ms = server_time_ms;

    ConnectResult result;
    result.initial_entity = MakeEntityPatch(stored_state, server_time_ms, server_tick);
    result.initial_batch = MakeReplicationBatch(stored_state, server_time_ms, server_tick, 0);
    return result;
}

std::optional<grpcmmo::world::v1::ReplicationBatch> AuthoritativeWorld::ApplyInput(
    const std::string& session_id, const grpcmmo::session::v1::InputFrame& input_frame,
    std::uint64_t heartbeat_interval_ms)
{
    std::scoped_lock lock(mutex_);

    const auto it = players_by_session_.find(session_id);
    if (it == players_by_session_.end())
    {
        return std::nullopt;
    }

    auto& player_state = it->second;
    const glm::dvec3 previous_position = player_state.position_m;
    const glm::dvec3 previous_facing_direction = player_state.facing_direction_unit;

    const auto& move = input_frame.move();
    double input_step_seconds = kDefaultInputStepSeconds;
    if (player_state.last_client_time_ms != 0 &&
        input_frame.client_time_ms() > player_state.last_client_time_ms)
    {
        input_step_seconds =
            static_cast<double>(input_frame.client_time_ms() - player_state.last_client_time_ms) /
            1000.0;
    }
    player_state.last_client_time_ms = input_frame.client_time_ms();
    input_step_seconds =
        std::clamp(input_step_seconds, kMinInputStepSeconds, kMaxInputStepSeconds);

    const double max_distance_m = kMoveSpeedMetersPerSecond * input_step_seconds;
    const glm::dvec3 requested_local_step =
        ClampLocalStep(glm::dvec3(move.world_displacement_m().x(),
                                  move.world_displacement_m().y(),
                                  move.world_displacement_m().z()),
                       max_distance_m);
    const double requested_distance = glm::length(requested_local_step);
    const glm::dvec3 current_up = SurfaceUpFromPosition(player_state.position_m);
    const glm::dvec3 requested_world_step =
        LocalVectorToWorldTangent(requested_local_step, current_up);

    if (move.has_facing_direction_unit())
    {
        player_state.facing_direction_unit =
            LocalDirectionToWorldTangent(glm::dvec3(move.facing_direction_unit().x(),
                                                    move.facing_direction_unit().y(),
                                                    move.facing_direction_unit().z()),
                                         current_up,
                                         player_state.facing_direction_unit);
    }
    else if (requested_distance > 0.000001)
    {
        player_state.facing_direction_unit =
            NormalizeOrXAxis(requested_world_step);
    }

    const glm::dvec3 moved_position = player_state.position_m + requested_world_step;
    player_state.position_m = terrain_sampler_.IsLoaded()
                                  ? terrain_sampler_.GroundWorldPosition(moved_position,
                                                                        planet_radius_m_)
                                  : ProjectToAltitude(moved_position,
                                                      planet_radius_m_,
                                                      PreviewPatchSurfaceAltitudeM(PreviewPatch()));
    player_state.facing_direction_unit =
        ProjectDirectionOntoTangent(player_state.facing_direction_unit,
                                    SurfaceUpFromPosition(player_state.position_m),
                                    previous_facing_direction);
    player_state.last_processed_input_sequence = input_frame.input_sequence();

    const std::uint64_t server_time_ms = grpcmmo::shared::NowMs();
    const bool changed = NearlyChanged(player_state.position_m, previous_position) ||
                         NearlyChanged(player_state.facing_direction_unit,
                                       previous_facing_direction);
    const bool heartbeat_due =
        server_time_ms >= (player_state.last_sent_time_ms + heartbeat_interval_ms);

    if (!changed && !heartbeat_due)
    {
        return std::nullopt;
    }

    const std::uint64_t server_tick = next_tick_++;
    player_state.last_sent_time_ms = server_time_ms;
    return MakeReplicationBatch(player_state, server_time_ms, server_tick,
                                player_state.last_processed_input_sequence);
}

void AuthoritativeWorld::DisconnectPlayer(const std::string& session_id)
{
    std::scoped_lock lock(mutex_);
    players_by_session_.erase(session_id);
}

grpcmmo::world::v1::EntityPatch AuthoritativeWorld::MakeEntityPatch(
    const PlayerState& player_state, std::uint64_t server_time_ms,
    std::uint64_t server_tick) const
{
    grpcmmo::world::v1::EntityPatch entity_patch;
    entity_patch.set_entity_id(player_state.entity_id);
    entity_patch.set_replication_mode(
        grpcmmo::world::v1::ENTITY_REPLICATION_MODE_FULL);
    entity_patch.set_sample_time_ms(server_time_ms);
    entity_patch.set_sample_tick(server_tick);
    entity_patch.set_interpolation_mode(
        grpcmmo::world::v1::INTERPOLATION_MODE_VELOCITY);

    auto* anchor = entity_patch.mutable_surface_anchor();
    anchor->set_planet_id(player_state.planet_id);
    anchor->set_zone_id(player_state.zone_id);
    anchor->set_patch_id(player_state.patch_id);
    anchor->set_altitude_m(
        AltitudeFromPosition(player_state.position_m, planet_radius_m_));
    const glm::dvec3 up = SurfaceUpFromPosition(player_state.position_m);
    anchor->mutable_unit_normal()->set_x(up.x);
    anchor->mutable_unit_normal()->set_y(up.y);
    anchor->mutable_unit_normal()->set_z(up.z);

    auto* transform = entity_patch.mutable_transform();
    transform->mutable_position_m()->set_x(player_state.position_m.x);
    transform->mutable_position_m()->set_y(player_state.position_m.y);
    transform->mutable_position_m()->set_z(player_state.position_m.z);
    SetOrientationFromForwardUp(transform->mutable_orientation(),
                                player_state.facing_direction_unit,
                                up);
    transform->mutable_up_unit()->set_x(up.x);
    transform->mutable_up_unit()->set_y(up.y);
    transform->mutable_up_unit()->set_z(up.z);

    auto* metadata = entity_patch.mutable_metadata();
    metadata->set_kind(grpcmmo::world::v1::ENTITY_KIND_PLAYER);
    metadata->set_archetype_id("player");
    metadata->set_display_name(player_state.character_name);
    metadata->set_bounding_radius_m(0.5);
    metadata->set_controlled_entity(true);

    auto* stats = entity_patch.mutable_stats();
    stats->set_health_ratio(1.0f);
    stats->set_energy_ratio(1.0f);
    stats->set_alive(true);

    return entity_patch;
}

grpcmmo::world::v1::ReplicationBatch AuthoritativeWorld::MakeReplicationBatch(
    const PlayerState& player_state, std::uint64_t server_time_ms,
    std::uint64_t server_tick, std::uint64_t last_processed_input_sequence)
{
    grpcmmo::world::v1::ReplicationBatch batch;
    batch.set_snapshot_id(next_snapshot_id_++);
    batch.set_base_snapshot_id(0);
    batch.set_server_time_ms(server_time_ms);
    batch.set_server_tick(server_tick);
    batch.set_last_processed_input_sequence(last_processed_input_sequence);
    *batch.add_entities() = MakeEntityPatch(player_state, server_time_ms, server_tick);
    return batch;
}
} // namespace grpcmmo::game
