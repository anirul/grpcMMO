#include <gtest/gtest.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>

#include "grpcmmo/game/AuthoritativeWorld.hpp"
#include "grpcmmo/shared/planet/PlanetMath.hpp"
#include "grpcmmo/shared/planet/PreviewPatchConfig.hpp"
#include "grpcmmo/shared/planet/PreviewPatchTerrainSampler.hpp"

namespace grpcmmo::game
{
namespace
{
using grpcmmo::shared::planet::AltitudeFromPosition;
using grpcmmo::shared::planet::BuildPreviewPatchFrame;
using grpcmmo::shared::planet::BuildPreviewPatchOriginPlanetPosition;
using grpcmmo::shared::planet::kMarsPreviewPatch000;
using grpcmmo::shared::planet::PreviewPatchSurfaceAltitudeM;
using grpcmmo::shared::planet::PreviewPatchTerrainSampler;
using grpcmmo::shared::planet::WorldDirectionToLocal;
using grpcmmo::shared::planet::WorldPositionToLocal;

ConnectedPlayer MakePlayer(
    const std::string& session_id = "session-1",
    const std::string& character_id = "char-1"
)
{
    ConnectedPlayer player;
    player.session_id = session_id;
    player.character_id = character_id;
    player.character_name = "Explorer";
    player.planet_id = "planet-dev";
    player.zone_id = "zone-dev";
    player.patch_id = "patch-000";
    return player;
}

glm::dvec3 ToDVec3(const grpcmmo::world::v1::Vector3d& value)
{
    return glm::dvec3(value.x(), value.y(), value.z());
}

glm::dquat ToDQuat(const grpcmmo::world::v1::Quaterniond& value)
{
    return glm::normalize(glm::dquat(value.w(), value.x(), value.y(), value.z())
    );
}

const grpcmmo::shared::planet::TangentFrame& PreviewPatchFrame()
{
    static const grpcmmo::shared::planet::TangentFrame frame =
        BuildPreviewPatchFrame(kMarsPreviewPatch000);
    return frame;
}

const glm::dvec3& PreviewPatchOriginPlanetPosition()
{
    static const glm::dvec3 origin_position =
        BuildPreviewPatchOriginPlanetPosition(kMarsPreviewPatch000);
    return origin_position;
}

glm::vec3 ToLocalDirection(const glm::dvec3& world_direction)
{
    const glm::dvec3 local_direction =
        WorldDirectionToLocal(world_direction, PreviewPatchFrame());
    return glm::normalize(glm::vec3(
        static_cast<float>(local_direction.x),
        static_cast<float>(local_direction.y),
        static_cast<float>(local_direction.z)
    ));
}

glm::dvec3 ToLocalPosition(const grpcmmo::world::v1::EntityPatch& patch)
{
    return WorldPositionToLocal(
        ToDVec3(patch.transform().position_m()),
        PreviewPatchOriginPlanetPosition(),
        PreviewPatchFrame()
    );
}

glm::vec3 ForwardFromPatch(const grpcmmo::world::v1::EntityPatch& patch)
{
    const glm::dvec3 world_forward = glm::rotate(
        ToDQuat(patch.transform().orientation()), glm::dvec3(1.0, 0.0, 0.0)
    );
    return ToLocalDirection(world_forward);
}

double ExpectedAltitudeFromPreviewTerrain(const glm::dvec3& position_m)
{
    static PreviewPatchTerrainSampler sampler = []
    {
        PreviewPatchTerrainSampler instance;
        (void)instance.Load(kMarsPreviewPatch000);
        return instance;
    }();

    if (!sampler.IsLoaded())
    {
        return PreviewPatchSurfaceAltitudeM(kMarsPreviewPatch000);
    }

    return sampler.SampleAbsoluteAltitudeM(position_m);
}
} // namespace

TEST(AuthoritativeWorldTest, ConnectPlayerReturnsControlledInitialEntity)
{
    AuthoritativeWorld world;
    const auto result = world.ConnectPlayer(MakePlayer());

    EXPECT_EQ(result.initial_entity.entity_id(), "entity-char-1");
    EXPECT_EQ(result.initial_entity.metadata().display_name(), "Explorer");
    EXPECT_EQ(
        result.initial_entity.metadata().kind(),
        grpcmmo::world::v1::ENTITY_KIND_PLAYER
    );
    EXPECT_TRUE(result.initial_entity.metadata().controlled_entity());
    ASSERT_EQ(result.initial_batch.entities_size(), 1);
    EXPECT_EQ(result.initial_batch.entities(0).entity_id(), "entity-char-1");
}

TEST(AuthoritativeWorldTest, ApplyInputClampsMovementToServerSpeedLimit)
{
    AuthoritativeWorld world;
    const auto connection = world.ConnectPlayer(MakePlayer());
    const glm::dvec3 initial_position =
        ToDVec3(connection.initial_entity.transform().position_m());
    const glm::dvec3 initial_local_position =
        ToLocalPosition(connection.initial_entity);

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);
    input_frame.mutable_move()->mutable_world_displacement_m()->set_x(100.0);

    const auto batch = world.ApplyInput("session-1", input_frame, 1000);
    ASSERT_TRUE(batch.has_value());
    ASSERT_EQ(batch->entities_size(), 1);

    const auto& moved_patch = batch->entities(0);
    const glm::dvec3 moved_position =
        ToDVec3(moved_patch.transform().position_m());
    const glm::dvec3 moved_local_position = ToLocalPosition(moved_patch);
    const glm::dvec3 local_delta =
        moved_local_position - initial_local_position;

    EXPECT_NEAR(
        glm::length(glm::dvec2(local_delta.x, local_delta.z)), 0.2, 0.0005
    );
    EXPECT_NEAR(local_delta.x, 0.2, 0.0005);
    EXPECT_NEAR(local_delta.z, 0.0, 0.0005);
    EXPECT_NEAR(
        AltitudeFromPosition(
            moved_position, kMarsPreviewPatch000.planet_radius_m
        ),
        ExpectedAltitudeFromPreviewTerrain(moved_position),
        0.0005
    );
}

TEST(AuthoritativeWorldTest, ApplyInputUsesFacingDirectionVectorWhenProvided)
{
    AuthoritativeWorld world;
    world.ConnectPlayer(MakePlayer());

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);
    auto* move = input_frame.mutable_move();
    move->mutable_world_displacement_m()->set_x(0.2);
    move->mutable_facing_direction_unit()->set_z(1.0);

    const auto batch = world.ApplyInput("session-1", input_frame, 1000);
    ASSERT_TRUE(batch.has_value());

    const glm::vec3 forward = ForwardFromPatch(batch->entities(0));
    EXPECT_NEAR(forward.x, 0.0f, 0.0001f);
    EXPECT_NEAR(forward.z, 1.0f, 0.0001f);
}

TEST(
    AuthoritativeWorldTest,
    ApplyInputFallsBackToMovementDirectionWhenFacingIsMissing
)
{
    AuthoritativeWorld world;
    world.ConnectPlayer(MakePlayer());

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);
    input_frame.mutable_move()->mutable_world_displacement_m()->set_z(-0.2);

    const auto batch = world.ApplyInput("session-1", input_frame, 1000);
    ASSERT_TRUE(batch.has_value());

    const glm::vec3 forward = ForwardFromPatch(batch->entities(0));
    EXPECT_NEAR(forward.x, 0.0f, 0.001f);
    EXPECT_NEAR(forward.z, -1.0f, 0.0001f);
}

TEST(AuthoritativeWorldTest, ApplyInputCanEmitHeartbeatWithoutMovement)
{
    AuthoritativeWorld world;
    world.ConnectPlayer(MakePlayer());

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);

    const auto batch = world.ApplyInput("session-1", input_frame, 0);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->last_processed_input_sequence(), 1);
    ASSERT_EQ(batch->entities_size(), 1);
    EXPECT_EQ(batch->entities(0).entity_id(), "entity-char-1");
}
} // namespace grpcmmo::game
