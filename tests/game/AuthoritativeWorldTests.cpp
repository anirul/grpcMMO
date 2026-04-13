#include <gtest/gtest.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>

#include "grpcmmo/game/AuthoritativeWorld.hpp"

namespace grpcmmo::game
{
namespace
{
ConnectedPlayer MakePlayer(const std::string& session_id = "session-1",
                           const std::string& character_id = "char-1")
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

glm::vec3 ForwardFromPatch(const grpcmmo::world::v1::EntityPatch& patch)
{
    const auto& orientation = patch.transform().orientation();
    glm::quat quaternion(static_cast<float>(orientation.w()),
                         static_cast<float>(orientation.x()),
                         static_cast<float>(orientation.y()),
                         static_cast<float>(orientation.z()));
    glm::vec3 forward = glm::rotate(glm::normalize(quaternion), glm::vec3(1.0f, 0.0f, 0.0f));
    forward.y = 0.0f;
    return glm::normalize(forward);
}
} // namespace

TEST(AuthoritativeWorldTest, ConnectPlayerReturnsControlledInitialEntity)
{
    AuthoritativeWorld world(1000.0);
    const auto result = world.ConnectPlayer(MakePlayer());

    EXPECT_EQ(result.initial_entity.entity_id(), "entity-char-1");
    EXPECT_EQ(result.initial_entity.metadata().display_name(), "Explorer");
    EXPECT_EQ(result.initial_entity.metadata().kind(), grpcmmo::world::v1::ENTITY_KIND_PLAYER);
    EXPECT_TRUE(result.initial_entity.metadata().controlled_entity());
    ASSERT_EQ(result.initial_batch.entities_size(), 1);
    EXPECT_EQ(result.initial_batch.entities(0).entity_id(), "entity-char-1");
}

TEST(AuthoritativeWorldTest, ApplyInputClampsMovementToServerSpeedLimit)
{
    AuthoritativeWorld world(1000.0);
    world.ConnectPlayer(MakePlayer());

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);
    input_frame.mutable_move()->mutable_world_displacement_m()->set_x(100.0);

    const auto batch = world.ApplyInput("session-1", input_frame, 1000);
    ASSERT_TRUE(batch.has_value());
    ASSERT_EQ(batch->entities_size(), 1);
    EXPECT_NEAR(batch->entities(0).transform().position_m().x(), 0.2, 0.000001);
    EXPECT_NEAR(batch->entities(0).transform().position_m().z(), 0.0, 0.000001);
}

TEST(AuthoritativeWorldTest, ApplyInputUsesFacingDirectionVectorWhenProvided)
{
    AuthoritativeWorld world(1000.0);
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

TEST(AuthoritativeWorldTest, ApplyInputFallsBackToMovementDirectionWhenFacingIsMissing)
{
    AuthoritativeWorld world(1000.0);
    world.ConnectPlayer(MakePlayer());

    grpcmmo::session::v1::InputFrame input_frame;
    input_frame.set_input_sequence(1);
    input_frame.set_client_time_ms(50);
    input_frame.mutable_move()->mutable_world_displacement_m()->set_z(-0.2);

    const auto batch = world.ApplyInput("session-1", input_frame, 1000);
    ASSERT_TRUE(batch.has_value());

    const glm::vec3 forward = ForwardFromPatch(batch->entities(0));
    EXPECT_NEAR(forward.x, 0.0f, 0.0001f);
    EXPECT_NEAR(forward.z, -1.0f, 0.0001f);
}

TEST(AuthoritativeWorldTest, ApplyInputCanEmitHeartbeatWithoutMovement)
{
    AuthoritativeWorld world(1000.0);
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
