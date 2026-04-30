#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "MoveCommand.hpp"
#include "auth/v1/auth.grpc.pb.h"
#include "session/v1/session.grpc.pb.h"

namespace grpcmmo::client
{
    struct ClientConnectionConfig
    {
        std::string auth_server_address = "127.0.0.1:50050";
        std::string login_name = "demo";
        std::string password = "demo";
        std::string realm_id = "realm-dev";
        std::string character_name = "explorer";
    };

    class GrpcSessionClient
    {
      public:
        GrpcSessionClient();
        ~GrpcSessionClient();

        bool Connect(
            const ClientConnectionConfig& config, std::string* error_message
        );
        bool SendMove(const MoveCommand& move_command);
        bool SendPing();
        void PollMessages(
            const std::function<
                void(const grpcmmo::session::v1::ServerMessage&)>& on_message
        );
        grpc::Status Shutdown();
        bool IsOpen() const;

      private:
        bool FetchOrCreateCharacter(
            const ClientConnectionConfig& config,
            const grpcmmo::auth::v1::LoginResponse& login_response,
            grpcmmo::auth::v1::CharacterSummary* character,
            std::string* error_message
        );
        bool OpenSessionStream(
            const ClientConnectionConfig& config,
            const grpcmmo::auth::v1::CharacterSummary& character,
            const grpcmmo::auth::v1::CreateSessionGrantResponse& grant_response,
            std::string* error_message
        );
        void StartReader();
        void StartWriter();
        bool EnqueueMessage(grpcmmo::session::v1::ClientMessage&& message);

        std::unique_ptr<grpcmmo::session::v1::SessionService::Stub>
            session_stub_;
        std::unique_ptr<grpc::ClientContext> session_context_;
        std::unique_ptr<grpc::ClientReaderWriter<
            grpcmmo::session::v1::ClientMessage,
            grpcmmo::session::v1::ServerMessage>>
            session_stream_;
        std::thread reader_thread_;
        std::thread writer_thread_;
        std::mutex write_mutex_;
        std::mutex queue_mutex_;
        std::mutex outgoing_queue_mutex_;
        std::condition_variable outgoing_queue_cv_;
        std::deque<grpcmmo::session::v1::ServerMessage> pending_messages_;
        std::deque<grpcmmo::session::v1::ClientMessage>
            pending_outgoing_messages_;
        std::atomic<bool> open_{false};
        std::atomic<bool> writer_stop_requested_{false};
        std::atomic<std::uint64_t> next_input_sequence_{1};
    };
} // namespace grpcmmo::client
