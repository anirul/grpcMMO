#include "Client.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "frame/common/application.h"
#include "frame/logger.h"

ABSL_FLAG(std::string, auth_server_address, "127.0.0.1:50050", "Auth server address.");
ABSL_FLAG(std::string, login_name, "demo", "Login name.");
ABSL_FLAG(std::string, password, "demo", "Login password.");
ABSL_FLAG(std::string, realm_id, "realm-dev", "Realm identifier.");
ABSL_FLAG(std::string, character_name, "explorer", "Character to use or create.");
ABSL_FLAG(int, window_width, 1280, "Window width.");
ABSL_FLAG(int, window_height, 720, "Window height.");
ABSL_FLAG(int, move_send_interval_ms, 50, "Movement input send interval in milliseconds.");
ABSL_FLAG(double, auto_move_seconds, 0.0, "Drive forward automatically for N seconds.");
ABSL_FLAG(bool, debug_pose_trace, false, "Log controlled pawn/body/marker positions.");

namespace grpcmmo::client
{
namespace
{
constexpr glm::uvec2 kDefaultWindowSize{1280u, 720u};

bool NearlyEqualOrientation(const glm::quat& lhs, const glm::quat& rhs)
{
    const float alignment = std::abs(glm::dot(glm::normalize(lhs), glm::normalize(rhs)));
    return alignment >= 0.99995f;
}

glm::vec3 ToRenderPosition(const grpcmmo::world::v1::EntityPatch& pawn_patch)
{
    return glm::vec3(static_cast<float>(pawn_patch.transform().position_m().x()),
                     0.0f,
                     static_cast<float>(pawn_patch.transform().position_m().z()));
}

glm::quat ToRenderOrientation(const grpcmmo::world::v1::EntityPatch& pawn_patch)
{
    const auto& orientation = pawn_patch.transform().orientation();
    return glm::normalize(glm::quat(static_cast<float>(orientation.w()),
                                    static_cast<float>(orientation.x()),
                                    static_cast<float>(orientation.y()),
                                    static_cast<float>(orientation.z())));
}
}

int Client::Run(int argc, char** argv)
{
    frame::common::Application app(argc, argv, kDefaultWindowSize);
    LoadFlags();

    std::string error_message;
    if (!session_.Connect(connection_config_, &error_message))
    {
        std::cerr << error_message << std::endl;
        return 1;
    }

    asset_bootstrap_.EnsureFrameAssetsAvailable();
    const auto level_json_path =
        asset_bootstrap_.WriteGeneratedLevelJson(scene_.BuildLevelProto());
    app.Startup(level_json_path);
    app.GetWindow().SetWindowTitle("grpcMMO - Third Person");

    controller_ = std::make_unique<Controller>();
    controller_ptr_ = controller_.get();
    app.GetWindow().SetInputInterface(std::move(controller_));
    InitObjects(app.GetWindow());

    running_ = true;
    session_ready_received_ = false;
    controlled_pawn_.Init();
    controlled_pawn_available_ = false;
    controlled_pawn_server_id_.clear();
    camera_boon_.Reset();
    pending_move_command_.Clear();
    last_sent_facing_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    last_sent_facing_orientation_valid_ = false;
    last_frame_time_ = std::chrono::steady_clock::now();
    last_move_sent_at_ = last_frame_time_;

    const int exit_code = static_cast<int>(app.Run([this, &window = app.GetWindow()]()
                                                   {
                                                       const auto now = std::chrono::steady_clock::now();
                                                       const float delta_seconds =
                                                           std::chrono::duration<float>(now - last_frame_time_).count();
                                                       last_frame_time_ = now;

                                                       PumpNetworkMessages();
                                                       if (!running_)
                                                       {
                                                           return false;
                                                       }

                                                       if (!Tick(window, delta_seconds))
                                                       {
                                                           return false;
                                                       }
                                                       if (!SendMoveIfDue())
                                                       {
                                                           running_ = false;
                                                           return false;
                                                       }
                                                       return running_;
                                                   }));
    EndObjects();
    return exit_code;
}

void Client::LoadFlags()
{
    connection_config_.auth_server_address = absl::GetFlag(FLAGS_auth_server_address);
    connection_config_.login_name = absl::GetFlag(FLAGS_login_name);
    connection_config_.password = absl::GetFlag(FLAGS_password);
    connection_config_.realm_id = absl::GetFlag(FLAGS_realm_id);
    connection_config_.character_name = absl::GetFlag(FLAGS_character_name);
    window_size_ =
        glm::uvec2(static_cast<unsigned int>(std::max(640, absl::GetFlag(FLAGS_window_width))),
                   static_cast<unsigned int>(std::max(360, absl::GetFlag(FLAGS_window_height))));
    move_send_interval_ =
        std::chrono::milliseconds(std::max(16, absl::GetFlag(FLAGS_move_send_interval_ms)));
    auto_move_duration_ =
        std::chrono::duration<float>(static_cast<float>(std::max(0.0, absl::GetFlag(FLAGS_auto_move_seconds))));
    debug_pose_trace_ = absl::GetFlag(FLAGS_debug_pose_trace);
    scene_.SetDebugPoseTrace(debug_pose_trace_);
    frame::Logger::GetInstance()->set_level(spdlog::level::warn);
    frame::Logger::GetInstance()->flush_on(spdlog::level::warn);
}

void Client::PumpNetworkMessages()
{
    session_.PollMessages([this](const grpcmmo::session::v1::ServerMessage& message)
                          {
                              switch (message.payload_case())
                              {
                              case grpcmmo::session::v1::ServerMessage::kSessionReady:
                                  if (!session_ready_received_)
                                  {
                                      session_ready_received_ = true;
                                      session_ready_at_ = std::chrono::steady_clock::now();
                                  }
                                  controlled_pawn_server_id_ =
                                      message.session_ready().controlled_entity_id();
                                  controlled_pawn_available_ = true;
                                  ApplyControlledPatch(
                                      message.session_ready().initial_controlled_entity());
                                  std::cout << "[session] ready pawn="
                                            << controlled_pawn_server_id_
                                            << " zone=" << message.session_ready().zone_id()
                                            << std::endl;
                                  break;
                              case grpcmmo::session::v1::ServerMessage::kReplication:
                                  for (const auto& pawn_patch : message.replication().entities())
                                  {
                                      if (pawn_patch.entity_id() == controlled_pawn_server_id_)
                                      {
                                          ApplyControlledPatch(pawn_patch);
                                          break;
                                      }
                                  }
                                  break;
                              case grpcmmo::session::v1::ServerMessage::kNotice:
                                  std::cout << "[notice] "
                                            << message.notice().code() << ": "
                                            << message.notice().message() << std::endl;
                                  if (message.notice().fatal())
                                  {
                                      running_ = false;
                                  }
                                  break;
                              default:
                                  break;
                              }
                          });

    if (!session_.IsOpen())
    {
        running_ = false;
    }
}

void Client::ApplyControlledPatch(const grpcmmo::world::v1::EntityPatch& pawn_patch)
{
    PawnSnapshot snapshot;
    snapshot.pawn_id = pawn_patch.entity_id();
    snapshot.display_name = pawn_patch.metadata().display_name();
    snapshot.position = ToRenderPosition(pawn_patch);
    snapshot.orientation = ToRenderOrientation(pawn_patch);
    snapshot.controlled = true;
    controlled_pawn_.ApplyReplication(snapshot);
}

void Client::InitObjects(frame::WindowInterface& window)
{
    if (controller_ptr_ != nullptr)
    {
        controller_ptr_->Init();
    }
    controlled_pawn_.Init();
    controlled_pawn_available_ = false;
    camera_boon_.Init();
    scene_.Init();
    scene_.Attach(&window);
    camera_.Attach(&window);
    camera_.Init();
}

void Client::EndObjects()
{
    controlled_pawn_.End();
    controlled_pawn_available_ = false;
    controlled_pawn_server_id_.clear();
    camera_.End();
    scene_.End();
    camera_boon_.End();
    if (controller_ptr_ != nullptr)
    {
        controller_ptr_->End();
    }
}

bool Client::Tick(frame::WindowInterface& window, float delta_seconds)
{
    if (controller_ptr_ != nullptr)
    {
        controller_ptr_->Tick(delta_seconds);
    }
    const Controller::FrameInput frame_input = BuildFrameInput();
    if (frame_input.exit_requested)
    {
        running_ = false;
        return false;
    }

    camera_boon_.Tick(delta_seconds);
    if (controller_ptr_ != nullptr)
    {
        controller_ptr_->DriveCamera(camera_boon_, frame_input);
    }

    if (Pawn* controlled_pawn = GetControlledPawn(); controlled_pawn != nullptr)
    {
        const glm::quat local_facing_orientation =
            glm::angleAxis(camera_boon_.GetYawRadians(), glm::vec3(0.0f, 1.0f, 0.0f));
        controlled_pawn->SetLocalFacingOrientation(local_facing_orientation);
        if (!last_sent_facing_orientation_valid_ ||
            !NearlyEqualOrientation(local_facing_orientation, last_sent_facing_orientation_))
        {
            MoveCommand facing_command;
            facing_command.SetFacingOrientation(local_facing_orientation);
            pending_move_command_.Accumulate(facing_command);
        }
        const MoveCommand move_command =
            controller_ptr_ != nullptr
                ? controller_ptr_->DrivePawn(
                      *controlled_pawn, camera_boon_, frame_input, delta_seconds)
                : MoveCommand{};
        pending_move_command_.Accumulate(move_command);
    }

    if (controlled_pawn_available_)
    {
        controlled_pawn_.Tick(delta_seconds);
    }

    scene_.SetControlledState(GetControlledPawn(), &camera_boon_);
    scene_.Tick(delta_seconds);
    camera_.SetPose(scene_.BuildCameraPose(GetControlledPawn(), camera_boon_));
    camera_.Tick(delta_seconds);
    return true;
}

bool Client::SendMoveIfDue()
{
    if (!controlled_pawn_available_ || controlled_pawn_server_id_.empty())
    {
        return true;
    }

    if (!pending_move_command_.HasSignal())
    {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if ((now - last_move_sent_at_) < move_send_interval_)
    {
        return true;
    }

    last_move_sent_at_ = now;
    const bool sent = session_.SendMove(pending_move_command_);
    if (sent)
    {
        if (pending_move_command_.has_facing_orientation)
        {
            last_sent_facing_orientation_ = pending_move_command_.facing_orientation;
            last_sent_facing_orientation_valid_ = true;
        }
        pending_move_command_.Clear();
    }
    return sent;
}

Controller::FrameInput Client::BuildFrameInput() const
{
    Controller::FrameInput frame_input =
        controller_ptr_ != nullptr ? controller_ptr_->ConsumeFrameInput()
                                   : Controller::FrameInput{};

    if (!session_ready_received_)
    {
        return frame_input;
    }

    const float elapsed_seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - session_ready_at_).count();
    const bool auto_move_active =
        auto_move_duration_.count() > 0.0f &&
        elapsed_seconds <= auto_move_duration_.count();
    if (auto_move_active)
    {
        frame_input.move_forward = 1.0f;
    }
    return frame_input;
}

Pawn* Client::GetControlledPawn()
{
    if (!controlled_pawn_available_)
    {
        return nullptr;
    }
    return &controlled_pawn_;
}

const Pawn* Client::GetControlledPawn() const
{
    if (!controlled_pawn_available_)
    {
        return nullptr;
    }
    return &controlled_pawn_;
}
} // namespace grpcmmo::client
