#include "Client.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

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
        asset_bootstrap_.WriteGeneratedLevelJson(client_world_.BuildLevelProto());
    app.Startup(level_json_path);
    app.GetWindow().SetWindowTitle("grpcMMO - Third Person");
    client_world_.Attach(app.GetWindow());
    client_world_.Init();

    running_ = true;
    last_frame_time_ = std::chrono::steady_clock::now();
    last_move_sent_at_ = last_frame_time_;

    const int exit_code = static_cast<int>(app.Run([this]()
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

                                                       if (!Tick(delta_seconds))
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
    client_world_.End();
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
    client_world_.Configure(
        ClientWorldConfig{auto_move_duration_,
                          absl::GetFlag(FLAGS_debug_pose_trace)});
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
                                  client_world_.OnSessionReady(message.session_ready());
                                  std::cout << "[session] ready pawn="
                                            << message.session_ready().controlled_entity_id()
                                            << " zone=" << message.session_ready().zone_id()
                                            << std::endl;
                                  break;
                              case grpcmmo::session::v1::ServerMessage::kReplication:
                                  client_world_.ApplyReplicationBatch(message.replication());
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

bool Client::Tick(float delta_seconds)
{
    client_world_.Tick(delta_seconds);
    if (client_world_.IsExitRequested())
    {
        running_ = false;
        return false;
    }
    return true;
}

bool Client::SendMoveIfDue()
{
    if (!client_world_.HasControlledPawn())
    {
        return true;
    }

    if (!client_world_.HasPendingMoveCommand())
    {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if ((now - last_move_sent_at_) < move_send_interval_)
    {
        return true;
    }

    last_move_sent_at_ = now;
    const bool sent = session_.SendMove(client_world_.GetPendingMoveCommand());
    if (sent)
    {
        client_world_.OnMoveSent();
    }
    return sent;
}
} // namespace grpcmmo::client
