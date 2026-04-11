#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "AssetBootstrap.hpp"
#include "Camera.hpp"
#include "CameraBoon.hpp"
#include "Controller.hpp"
#include "GrpcSessionClient.hpp"
#include "MoveCommand.hpp"
#include "Pawn.hpp"
#include "Scene.hpp"
#include "world/v1/replication.pb.h"

namespace frame
{
namespace common
{
class Application;
}
class WindowInterface;
} // namespace frame

namespace grpcmmo::client
{
class Client
{
public:
    int Run(int argc, char** argv);

private:
    void LoadFlags();
    void PumpNetworkMessages();
    void InitObjects(frame::WindowInterface& window);
    void EndObjects();
    void ApplyControlledPatch(const grpcmmo::world::v1::EntityPatch& pawn_patch);
    bool Tick(frame::WindowInterface& window, float delta_seconds);
    bool SendMoveIfDue();
    Controller::FrameInput BuildFrameInput() const;
    Pawn* GetControlledPawn();
    const Pawn* GetControlledPawn() const;

    ClientConnectionConfig connection_config_;
    glm::uvec2 window_size_{1280u, 720u};
    std::chrono::milliseconds move_send_interval_{50};
    std::chrono::duration<float> auto_move_duration_{0.0f};
    std::chrono::steady_clock::time_point session_ready_at_{};
    std::chrono::steady_clock::time_point last_frame_time_{};
    std::chrono::steady_clock::time_point last_move_sent_at_{};
    glm::quat last_sent_facing_orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    MoveCommand pending_move_command_{};
    bool last_sent_facing_orientation_valid_ = false;
    bool debug_pose_trace_ = false;
    bool running_ = true;
    bool session_ready_received_ = false;
    AssetBootstrap asset_bootstrap_{};
    Scene scene_{};
    GrpcSessionClient session_;
    std::unique_ptr<Controller> controller_;
    Controller* controller_ptr_ = nullptr;
    CameraBoon camera_boon_;
    Camera camera_;
    Pawn controlled_pawn_{};
    bool controlled_pawn_available_ = false;
    std::string controlled_pawn_server_id_;
};
} // namespace grpcmmo::client
