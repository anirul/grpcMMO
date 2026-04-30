#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "grpcmmo/shared/planet/PreviewPatchConfig.hpp"
#include "grpcmmo/shared/planet/PreviewPatchTerrainSampler.hpp"
#include "session/v1/session.pb.h"
#include "world/v1/replication.pb.h"

namespace grpcmmo::game
{
    struct ConnectedPlayer
    {
        std::string session_id;
        std::string character_id;
        std::string character_name;
        std::string planet_id;
        std::string zone_id;
        std::string patch_id;
    };

    class AuthoritativeWorld
    {
      public:
        struct ConnectResult
        {
            grpcmmo::world::v1::EntityPatch initial_entity;
            grpcmmo::world::v1::ReplicationBatch initial_batch;
        };

        explicit AuthoritativeWorld(
            double planet_radius_m =
                grpcmmo::shared::planet::kMarsPreviewPatch000.planet_radius_m
        );

        ConnectResult ConnectPlayer(const ConnectedPlayer& player);
        std::optional<grpcmmo::world::v1::ReplicationBatch> ApplyInput(
            const std::string& session_id,
            const grpcmmo::session::v1::InputFrame& input_frame,
            std::uint64_t heartbeat_interval_ms
        );
        void DisconnectPlayer(const std::string& session_id);

      private:
        struct PlayerState
        {
            std::string session_id;
            std::string character_id;
            std::string character_name;
            std::string entity_id;
            std::string planet_id;
            std::string zone_id;
            std::string patch_id;
            glm::dvec3 position_m = glm::dvec3(0.0);
            glm::dvec3 facing_direction_unit = glm::dvec3(1.0, 0.0, 0.0);
            std::uint64_t last_client_time_ms = 0;
            std::uint64_t last_processed_input_sequence = 0;
            std::uint64_t last_sent_time_ms = 0;
        };

        grpcmmo::world::v1::EntityPatch MakeEntityPatch(
            const PlayerState& player_state,
            std::uint64_t server_time_ms,
            std::uint64_t server_tick
        ) const;
        grpcmmo::world::v1::ReplicationBatch MakeReplicationBatch(
            const PlayerState& player_state,
            std::uint64_t server_time_ms,
            std::uint64_t server_tick,
            std::uint64_t last_processed_input_sequence
        );

        double planet_radius_m_;
        grpcmmo::shared::planet::PreviewPatchTerrainSampler terrain_sampler_{};
        mutable std::mutex mutex_;
        std::unordered_map<std::string, PlayerState> players_by_session_;
        std::uint64_t next_snapshot_id_ = 1;
        std::uint64_t next_tick_ = 1;
    };
} // namespace grpcmmo::game
