#include "grpcmmo/game/AuthoritativeWorld.hpp"

#include <algorithm>
#include <cmath>

#include "grpcmmo/shared/Time.hpp"

namespace grpcmmo::game
{
namespace
{
constexpr double kMoveSpeedMetersPerSecond = 4.0;
constexpr double kDefaultInputStepSeconds = 0.05;
constexpr double kMinInputStepSeconds = 1.0 / 120.0;
constexpr double kMaxInputStepSeconds = 0.10;
constexpr double kSpawnSpacingMeters = 6.0;

template <typename PlayerStateT>
void SetFacingDirectionFromYaw(PlayerStateT* player_state, double yaw_radians)
{
    player_state->facing_direction_x = std::cos(yaw_radians);
    player_state->facing_direction_z = std::sin(yaw_radians);
}

template <typename PlayerStateT>
void SetFacingDirection(PlayerStateT* player_state,
                        double direction_x,
                        double direction_z)
{
    const double magnitude_squared =
        (direction_x * direction_x) + (direction_z * direction_z);
    if (magnitude_squared <= 0.000001)
    {
        return;
    }

    const double magnitude = std::sqrt(magnitude_squared);
    player_state->facing_direction_x = direction_x / magnitude;
    player_state->facing_direction_z = direction_z / magnitude;
}

template <typename OrientationT>
void SetOrientationFromFacingDirection(OrientationT* orientation,
                                       double direction_x,
                                       double direction_z)
{
    const double magnitude_squared =
        (direction_x * direction_x) + (direction_z * direction_z);
    if (magnitude_squared <= 0.000001)
    {
        orientation->set_x(0.0);
        orientation->set_y(0.0);
        orientation->set_z(0.0);
        orientation->set_w(1.0);
        return;
    }

    const double yaw_radians = std::atan2(direction_z, direction_x);
    orientation->set_x(0.0);
    orientation->set_y(-std::sin(yaw_radians * 0.5));
    orientation->set_z(0.0);
    orientation->set_w(std::cos(yaw_radians * 0.5));
}
} // namespace

AuthoritativeWorld::AuthoritativeWorld(double planet_radius_m)
    : planet_radius_m_(planet_radius_m)
{
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
    player_state.x_m = static_cast<double>(spawn_index) * kSpawnSpacingMeters;
    player_state.y_m = planet_radius_m_ + 2.0;
    player_state.z_m = 0.0;
    SetFacingDirectionFromYaw(&player_state, 0.0);

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
    const double previous_x = player_state.x_m;
    const double previous_z = player_state.z_m;
    const double previous_facing_direction_x = player_state.facing_direction_x;
    const double previous_facing_direction_z = player_state.facing_direction_z;

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
    double requested_dx = move.world_displacement_m().x();
    double requested_dz = move.world_displacement_m().z();
    const double requested_distance =
        std::sqrt((requested_dx * requested_dx) + (requested_dz * requested_dz));
    if (requested_distance > max_distance_m && requested_distance > 0.000001)
    {
        const double scale = max_distance_m / requested_distance;
        requested_dx *= scale;
        requested_dz *= scale;
    }

    if (move.has_facing_direction_unit())
    {
        SetFacingDirection(&player_state,
                           move.facing_direction_unit().x(),
                           move.facing_direction_unit().z());
    }
    else if (requested_distance > 0.000001)
    {
        SetFacingDirection(&player_state, requested_dx, requested_dz);
    }

    player_state.x_m += requested_dx;
    player_state.z_m += requested_dz;
    player_state.last_processed_input_sequence = input_frame.input_sequence();

    const std::uint64_t server_time_ms = grpcmmo::shared::NowMs();
    const bool changed =
        std::abs(player_state.x_m - previous_x) > 0.0001 ||
        std::abs(player_state.z_m - previous_z) > 0.0001 ||
        std::abs(player_state.facing_direction_x - previous_facing_direction_x) > 0.0001 ||
        std::abs(player_state.facing_direction_z - previous_facing_direction_z) > 0.0001;
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
    anchor->set_altitude_m(2.0);
    anchor->mutable_unit_normal()->set_x(0.0);
    anchor->mutable_unit_normal()->set_y(1.0);
    anchor->mutable_unit_normal()->set_z(0.0);

    auto* transform = entity_patch.mutable_transform();
    transform->mutable_position_m()->set_x(player_state.x_m);
    transform->mutable_position_m()->set_y(player_state.y_m);
    transform->mutable_position_m()->set_z(player_state.z_m);
    SetOrientationFromFacingDirection(transform->mutable_orientation(),
                                      player_state.facing_direction_x,
                                      player_state.facing_direction_z);
    transform->mutable_up_unit()->set_x(0.0);
    transform->mutable_up_unit()->set_y(1.0);
    transform->mutable_up_unit()->set_z(0.0);

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
