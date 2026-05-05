#include <gtest/gtest.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

#include "ActorFactory.hpp"
#include "CameraBoon.hpp"
#include "CameraDirector.hpp"
#include "Character.hpp"
#include "FrameSceneBridge.hpp"
#include "Pawn.hpp"
#include "PlayerController.hpp"
#include "TerrainPatchSampler.hpp"
#include "WorldActor.hpp"
#include "frame/json/proto.h"
#include "grpcmmo/shared/WorkspaceConfig.hpp"
#include "world/v1/replication.pb.h"

namespace grpcmmo::client
{
    namespace
    {
        constexpr float kPositionTolerance = 0.0001f;

        void ExpectVec3Near(
            const glm::vec3& actual,
            const glm::vec3& expected,
            float tolerance = kPositionTolerance
        )
        {
            EXPECT_NEAR(actual.x, expected.x, tolerance);
            EXPECT_NEAR(actual.y, expected.y, tolerance);
            EXPECT_NEAR(actual.z, expected.z, tolerance);
        }

        grpcmmo::world::v1::EntityPatch MakeEntityPatch(
            grpcmmo::world::v1::EntityKind kind,
            bool controlled_entity,
            const std::string& entity_id = "entity-1",
            const std::string& display_name = "Entity"
        )
        {
            grpcmmo::world::v1::EntityPatch patch;
            patch.set_entity_id(entity_id);
            auto* metadata = patch.mutable_metadata();
            metadata->set_kind(kind);
            metadata->set_controlled_entity(controlled_entity);
            metadata->set_display_name(display_name);
            return patch;
        }

        PawnSnapshot MakeControlledSnapshot()
        {
            PawnSnapshot snapshot;
            snapshot.pawn_id = "pawn-1";
            snapshot.display_name = "Explorer";
            snapshot.position = glm::vec3(0.0f, 0.0f, 0.0f);
            snapshot.facing_direction = glm::vec3(1.0f, 0.0f, 0.0f);
            snapshot.controlled = true;
            return snapshot;
        }

        bool HasMatrixNode(
            const frame::proto::Level& level, const std::string& name
        )
        {
            for (const auto& node : level.scene_tree().node_matrices())
            {
                if (node.name() == name)
                {
                    return true;
                }
            }
            return false;
        }

        bool HasMeshNode(
            const frame::proto::Level& level, const std::string& name
        )
        {
            for (const auto& node : level.scene_tree().node_meshes())
            {
                if (node.name() == name)
                {
                    return true;
                }
            }
            return false;
        }

        bool HasCameraNode(
            const frame::proto::Level& level, const std::string& name
        )
        {
            for (const auto& node : level.scene_tree().node_cameras())
            {
                if (node.name() == name)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    TEST(ActorFactoryTest, CreatesTypedReplicatedActorsFromPatchMetadata)
    {
        ActorFactory factory;

        auto controlled_player = factory.CreateReplicatedActor(MakeEntityPatch(
            grpcmmo::world::v1::ENTITY_KIND_PLAYER, true, "player-1", "Player"
        ));
        ASSERT_NE(controlled_player, nullptr);
        EXPECT_NE(
            dynamic_cast<PlayerCharacter*>(controlled_player.get()), nullptr
        );
        EXPECT_TRUE(controlled_player->IsReplicated());
        EXPECT_EQ(controlled_player->GetEntityId(), "player-1");
        EXPECT_EQ(controlled_player->GetDisplayName(), "Player");

        auto npc = factory.CreateReplicatedActor(MakeEntityPatch(
            grpcmmo::world::v1::ENTITY_KIND_NPC, false, "npc-1", "Npc"
        ));
        ASSERT_NE(npc, nullptr);
        EXPECT_NE(dynamic_cast<NpcCharacter*>(npc.get()), nullptr);

        auto interactive = factory.CreateReplicatedActor(MakeEntityPatch(
            grpcmmo::world::v1::ENTITY_KIND_INTERACTIVE, false, "door-1", "Door"
        ));
        ASSERT_NE(interactive, nullptr);
        EXPECT_NE(
            dynamic_cast<InteractivePropActor*>(interactive.get()), nullptr
        );
    }

    TEST(CameraDirectorTest, TracksMultipleCamerasAndSwitchesActivePose)
    {
        CameraDirector director;
        director.Init();

        CameraActor& gameplay_camera = director.CreateCamera("gameplay");
        CameraPose gameplay_pose;
        gameplay_pose.position = glm::vec3(-5.0f, 3.0f, -2.0f);
        gameplay_pose.target = glm::vec3(0.0f, 0.0f, 0.0f);
        gameplay_camera.SetPose(gameplay_pose);

        CameraActor& cinematic_camera = director.CreateCamera("cinematic");
        CameraPose cinematic_pose;
        cinematic_pose.position = glm::vec3(9.0f, 6.0f, 4.0f);
        cinematic_pose.target = glm::vec3(1.0f, 1.0f, 1.0f);
        cinematic_camera.SetPose(cinematic_pose);

        EXPECT_EQ(director.GetActiveCamera(), &gameplay_camera);
        EXPECT_TRUE(director.SetActiveCamera("cinematic"));
        EXPECT_EQ(director.GetActiveCamera(), &cinematic_camera);

        CameraPose active_pose;
        ASSERT_TRUE(director.TryGetActiveCameraPose(&active_pose));
        ExpectVec3Near(active_pose.position, cinematic_pose.position);
        ExpectVec3Near(active_pose.target, cinematic_pose.target);

        director.End();
    }

    TEST(PlayerControllerTest, NormalizesDiagonalMovementAndClampsMovementStep)
    {
        PlayerController controller;
        controller.Init();

        Pawn pawn;
        pawn.Init();
        pawn.ApplyReplication(MakeControlledSnapshot());
        controller.Possess(&pawn);

        ASSERT_TRUE(controller.KeyPressed('w', 0.0));
        ASSERT_TRUE(controller.KeyPressed('d', 0.0));
        controller.Tick(1.0f / 60.0f);

        const PlayerController::FrameInput frame_input =
            controller.ConsumeFrameInput();
        const glm::vec2 movement(
            frame_input.move_forward, frame_input.move_right
        );
        EXPECT_NEAR(glm::length(movement), 1.0f, 0.0001f);

        const MoveCommand move_command =
            controller.DrivePawn(frame_input, 1.0f);
        const glm::dvec2 displacement(
            move_command.world_displacement_m.x,
            move_command.world_displacement_m.z
        );
        EXPECT_NEAR(glm::length(displacement), 0.2f, 0.0001f);

        controller.End();
        pawn.End();
    }

    TEST(
        PlayerControllerTest, MouseOrbitUsesDragSignsOnlyWhileOrbitButtonIsHeld
    )
    {
        PlayerController controller;
        controller.Init();

        ASSERT_TRUE(
            controller.MouseMoved(glm::vec2(0.0f), glm::vec2(12.0f, 6.0f), 0.0)
        );
        controller.Tick(1.0f / 60.0f);
        PlayerController::FrameInput frame_input =
            controller.ConsumeFrameInput();
        EXPECT_FLOAT_EQ(frame_input.look_yaw_delta_radians, 0.0f);
        EXPECT_FLOAT_EQ(frame_input.look_pitch_delta_radians, 0.0f);

        ASSERT_TRUE(controller.MousePressed(19, 0.0));
        ASSERT_TRUE(
            controller.MouseMoved(glm::vec2(0.0f), glm::vec2(10.0f, 8.0f), 0.0)
        );
        controller.Tick(1.0f / 60.0f);
        frame_input = controller.ConsumeFrameInput();
        EXPECT_GT(frame_input.look_yaw_delta_radians, 0.0f);
        EXPECT_GT(frame_input.look_pitch_delta_radians, 0.0f);

        ASSERT_TRUE(controller.MouseReleased(19, 0.0));
        ASSERT_TRUE(
            controller.MouseMoved(glm::vec2(0.0f), glm::vec2(10.0f, 8.0f), 0.0)
        );
        controller.Tick(1.0f / 60.0f);
        frame_input = controller.ConsumeFrameInput();
        EXPECT_FLOAT_EQ(frame_input.look_yaw_delta_radians, 0.0f);
        EXPECT_FLOAT_EQ(frame_input.look_pitch_delta_radians, 0.0f);

        controller.End();
    }

    TEST(PlayerControllerTest, DrivePawnFollowsCameraYawForForwardMovement)
    {
        PlayerController controller;
        controller.Init();

        Pawn pawn;
        pawn.Init();
        pawn.ApplyReplication(MakeControlledSnapshot());
        controller.Possess(&pawn);

        PlayerController::FrameInput look_input;
        look_input.look_yaw_delta_radians = glm::half_pi<float>();
        controller.DriveCamera(look_input);

        PlayerController::FrameInput move_input;
        move_input.move_forward = 1.0f;
        const MoveCommand move_command =
            controller.DrivePawn(move_input, 0.05f);

        EXPECT_NEAR(move_command.world_displacement_m.x, 0.0, 0.0001);
        EXPECT_GT(move_command.world_displacement_m.z, 0.19);

        controller.End();
        pawn.End();
    }

    TEST(PawnTest, UncontrolledReplicationSnapsAndIgnoresLocalMove)
    {
        Pawn pawn;
        pawn.Init();

        PawnSnapshot snapshot;
        snapshot.pawn_id = "remote-1";
        snapshot.display_name = "Remote";
        snapshot.position = glm::vec3(2.0f, 0.0f, 5.0f);
        snapshot.facing_direction = glm::vec3(0.0f, 0.0f, 1.0f);
        snapshot.controlled = false;
        pawn.ApplyReplication(snapshot);

        ExpectVec3Near(pawn.GetRenderPosition(), snapshot.position);
        ExpectVec3Near(
            pawn.GetRenderFacingDirection(), snapshot.facing_direction
        );

        MoveCommand move_command;
        move_command.world_displacement_m = glm::dvec3(3.0, 0.0, 0.0);
        pawn.ApplyMove(move_command);
        ExpectVec3Near(pawn.GetRenderPosition(), snapshot.position);

        pawn.End();
    }

    TEST(
        PawnTest,
        ControlledPawnAppliesLocalFacingAndKeepsPredictionBeforeIdleCorrection
    )
    {
        Pawn pawn;
        pawn.Init();

        pawn.ApplyReplication(MakeControlledSnapshot());
        pawn.SetLocalFacingDirection(glm::vec3(0.0f, 0.0f, 1.0f));
        ExpectVec3Near(
            pawn.GetRenderFacingDirection(), glm::vec3(0.0f, 0.0f, 1.0f)
        );
        EXPECT_NEAR(pawn.GetRenderYawRadians(), glm::half_pi<float>(), 0.0001f);

        MoveCommand move_command;
        move_command.world_displacement_m = glm::dvec3(1.0, 0.0, 0.0);
        pawn.ApplyMove(move_command);
        EXPECT_NEAR(pawn.GetRenderPosition().x, 1.0f, 0.0001f);

        PawnSnapshot authoritative_snapshot = MakeControlledSnapshot();
        authoritative_snapshot.position = glm::vec3(0.0f, 0.0f, 0.0f);
        pawn.ApplyReplication(authoritative_snapshot);
        pawn.Tick(0.05f);

        EXPECT_NEAR(pawn.GetRenderPosition().x, 1.0f, 0.0001f);
        pawn.End();
    }

    TEST(TerrainPatchSamplerTest, GroundsPreviewPatchCenterToSurface)
    {
        if (!grpcmmo::shared::kHaveDataRepo)
        {
            GTEST_SKIP(
            ) << "grpcMMO-data repo is not configured for this workspace";
        }

        TerrainPatchSampler sampler;
        ASSERT_TRUE(sampler.LoadPreviewPatch());

        const glm::vec3 grounded =
            sampler.GroundLocalPosition(glm::vec3(0.0f, 12.0f, 0.0f));
        ExpectVec3Near(grounded, glm::vec3(0.0f, 0.0f, 0.0f), 0.0005f);
    }

    TEST(FrameSceneBridgeTest, BuildFollowCameraPoseTargetsControlledPawn)
    {
        FrameSceneBridge bridge;
        CameraBoon camera_boon;
        camera_boon.Init();

        Pawn pawn;
        pawn.Init();
        PawnSnapshot snapshot = MakeControlledSnapshot();
        snapshot.position = glm::vec3(10.0f, 0.0f, 20.0f);
        snapshot.facing_direction = glm::vec3(0.0f, 0.0f, 1.0f);
        pawn.ApplyReplication(snapshot);

        const CameraPose pose =
            bridge.BuildFollowCameraPose(&pawn, camera_boon);
        ExpectVec3Near(
            pose.target,
            glm::vec3(10.0f, camera_boon.GetFocusHeightMeters(), 20.0f)
        );
        EXPECT_LT(pose.position.x, pose.target.x);
        EXPECT_GT(pose.position.y, pose.target.y);
        ExpectVec3Near(pose.up, glm::vec3(0.0f, 1.0f, 0.0f));

        pawn.End();
        camera_boon.End();
    }

    TEST(FrameSceneBridgeTest, BuildLevelProtoContainsGameplayScaffoldingNodes)
    {
        FrameSceneBridge bridge;
        const frame::proto::Level level = bridge.BuildLevelProto();

        EXPECT_EQ(level.name(), "grpcMMOThirdPerson");
        EXPECT_TRUE(HasMatrixNode(level, "ground_holder"));
        EXPECT_TRUE(HasMatrixNode(level, "pawn_root_matrix"));
        EXPECT_TRUE(HasMatrixNode(level, "camera_boon_matrix"));
        EXPECT_TRUE(HasMeshNode(level, "GroundMesh"));
        EXPECT_TRUE(HasMeshNode(level, "PawnBodyMesh"));
        EXPECT_TRUE(HasCameraNode(level, "camera"));
    }
} // namespace grpcmmo::client
