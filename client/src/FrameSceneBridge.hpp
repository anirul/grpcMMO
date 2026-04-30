#pragma once

#include <array>
#include <chrono>

#include "Camera.hpp"
#include "CameraBoon.hpp"
#include "Object.hpp"
#include "Pawn.hpp"
#include "frame/entity_id.h"

namespace frame
{
class LevelInterface;
class WindowInterface;
namespace proto
{
class Level;
}
} // namespace frame

namespace grpcmmo::client
{
class FrameSceneBridge : public Object
{
  public:
    void Attach(frame::WindowInterface* window);
    void SetViewState(
        const Pawn* controlled_pawn, const CameraBoon* camera_boon
    );

    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    void SetDebugPoseTrace(bool enabled);
    [[nodiscard]] CameraPose BuildFollowCameraPose(
        const Pawn* controlled_pawn, const CameraBoon& camera_boon
    ) const;
    [[nodiscard]] frame::proto::Level BuildLevelProto() const;

  private:
    static constexpr int kGroundGridRadius = 4;
    static constexpr std::size_t kGroundTileCount = static_cast<std::size_t>(
        (kGroundGridRadius * 2 + 1) * (kGroundGridRadius * 2 + 1)
    );
    static constexpr int kGuideLineHalfCount = 6;
    static constexpr std::size_t kGuideLineCount =
        static_cast<std::size_t>((kGuideLineHalfCount * 2 + 1) * 2);

    void CacheHandles(frame::LevelInterface& level);
    [[nodiscard]] glm::vec3 BuildCameraForwardOnGround(
        const Pawn* controlled_pawn, const CameraBoon& camera_boon
    ) const;
    [[nodiscard]] glm::vec3 BuildCameraBoonLocalOffset(
        const CameraBoon& camera_boon
    ) const;
    void SetNodeMatrixIfChanged(
        frame::LevelInterface& level,
        frame::EntityId node_id,
        const glm::mat4& matrix,
        glm::mat4* cached_matrix,
        bool* cached
    ) const;
    void UpdatePawnRoot(
        frame::LevelInterface& level,
        const Pawn* controlled_pawn,
        const CameraBoon& camera_boon
    ) const;
    void UpdateWorldHolders(
        frame::LevelInterface& level, const Pawn* controlled_pawn
    ) const;

    frame::WindowInterface* window_ = nullptr;
    const Pawn* controlled_pawn_ = nullptr;
    const CameraBoon* camera_boon_ = nullptr;
    bool debug_pose_trace_ = false;
    bool scene_handles_cached_ = false;
    mutable bool world_holders_initialized_ = false;
    mutable bool pawn_root_matrix_cached_ = false;
    mutable bool camera_boon_matrix_cached_ = false;
    mutable std::chrono::steady_clock::time_point last_pose_trace_at_{};
    mutable glm::mat4 cached_pawn_root_matrix_{1.0f};
    mutable glm::mat4 cached_camera_boon_matrix_{1.0f};
    frame::EntityId ground_holder_matrix_id_ = frame::NullId;
    frame::EntityId guide_holder_matrix_id_ = frame::NullId;
    frame::EntityId landmark_holder_matrix_id_ = frame::NullId;
    frame::EntityId pawn_root_matrix_id_ = frame::NullId;
    frame::EntityId camera_boon_matrix_id_ = frame::NullId;
};
} // namespace grpcmmo::client
