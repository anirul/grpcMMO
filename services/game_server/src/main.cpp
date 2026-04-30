#include <grpcpp/grpcpp.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "grpcmmo/game/AuthoritativeWorld.hpp"
#include "grpcmmo/shared/Time.hpp"
#include "grpcmmo/storage/SqliteStorage.hpp"
#include "session/v1/session.grpc.pb.h"

ABSL_FLAG(
    std::string, listen_address, "0.0.0.0:50051", "Game server listen address."
);
ABSL_FLAG(
    std::string, db_path, "data/grpcmmo.sqlite3", "SQLite database path."
);
ABSL_FLAG(
    uint64_t,
    snapshot_send_interval_ms,
    100,
    "Replication interval hint sent to the client."
);
ABSL_FLAG(
    uint64_t,
    interpolation_back_time_ms,
    150,
    "Interpolation back-time hint sent to the client."
);
ABSL_FLAG(
    uint64_t,
    heartbeat_interval_ms,
    1000,
    "Send a replication heartbeat if no motion occurs within this interval."
);

namespace
{
    class SessionServiceImpl final
        : public grpcmmo::session::v1::SessionService::Service
    {
      public:
        SessionServiceImpl(
            grpcmmo::storage::SqliteStorage* storage,
            grpcmmo::game::AuthoritativeWorld* world,
            std::uint64_t snapshot_send_interval_ms,
            std::uint64_t interpolation_back_time_ms,
            std::uint64_t heartbeat_interval_ms
        )
            : storage_(storage), world_(world),
              snapshot_send_interval_ms_(snapshot_send_interval_ms),
              interpolation_back_time_ms_(interpolation_back_time_ms),
              heartbeat_interval_ms_(heartbeat_interval_ms)
        {
        }

        grpc::Status OpenSession(
            grpc::ServerContext*,
            grpc::ServerReaderWriter<
                grpcmmo::session::v1::ServerMessage,
                grpcmmo::session::v1::ClientMessage>* stream
        ) override
        {
            grpcmmo::session::v1::ClientMessage first_message;
            if (!stream->Read(&first_message))
            {
                std::cout << "[game] open_session failed: missing begin_session"
                          << std::endl;
                return grpc::Status(
                    grpc::StatusCode::FAILED_PRECONDITION,
                    "begin_session required"
                );
            }

            if (!first_message.has_begin_session())
            {
                grpcmmo::session::v1::ServerMessage notice_message;
                auto* notice = notice_message.mutable_notice();
                notice->set_code("protocol_error");
                notice->set_message(
                    "the first client message must be begin_session"
                );
                notice->set_fatal(true);
                stream->Write(notice_message);
                std::cout << "[game] protocol_error: first message was not "
                             "begin_session"
                          << std::endl;
                return grpc::Status::OK;
            }

            const auto grant = storage_->FindSessionGrant(
                first_message.begin_session().session_token()
            );
            if (!grant.has_value() ||
                grant->character_id !=
                    first_message.begin_session().character_id())
            {
                grpcmmo::session::v1::ServerMessage notice_message;
                auto* notice = notice_message.mutable_notice();
                notice->set_code("invalid_session");
                notice->set_message(
                    "session token was not found or did not match the character"
                );
                notice->set_fatal(true);
                stream->Write(notice_message);
                std::cout << "[game] invalid_session character="
                          << first_message.begin_session().character_id()
                          << std::endl;
                return grpc::Status::OK;
            }

            grpcmmo::game::ConnectedPlayer player;
            player.session_id = grant->session_id;
            player.character_id = grant->character_id;
            player.character_name = grant->character_name;
            player.planet_id = grant->planet_id;
            player.zone_id = grant->zone_id;
            player.patch_id = "patch-000";

            const auto connection = world_->ConnectPlayer(player);
            std::cout << "[game] session_open session=" << grant->session_id
                      << " character=" << grant->character_id
                      << " zone=" << grant->zone_id << std::endl;

            grpcmmo::session::v1::ServerMessage ready_message;
            auto* ready = ready_message.mutable_session_ready();
            ready->set_connection_id("conn-" + grant->session_id);
            ready->set_session_id(grant->session_id);
            ready->set_controlled_entity_id(connection.initial_entity.entity_id(
            ));
            ready->set_planet_id(grant->planet_id);
            ready->set_zone_id(grant->zone_id);
            ready->set_snapshot_send_interval_ms_hint(snapshot_send_interval_ms_
            );
            ready->set_interpolation_back_time_ms_hint(
                interpolation_back_time_ms_
            );
            ready->set_heartbeat_interval_ms_hint(heartbeat_interval_ms_);
            *ready->mutable_initial_controlled_entity() =
                connection.initial_entity;
            stream->Write(ready_message);

            grpcmmo::session::v1::ServerMessage initial_replication;
            *initial_replication.mutable_replication() =
                connection.initial_batch;
            stream->Write(initial_replication);

            grpcmmo::session::v1::ClientMessage client_message;
            while (stream->Read(&client_message))
            {
                if (client_message.has_input_frame())
                {
                    std::cout << "[game] input session=" << grant->session_id
                              << " seq="
                              << client_message.input_frame().input_sequence()
                              << " dx="
                              << client_message.input_frame()
                                     .move()
                                     .world_displacement_m()
                                     .x()
                              << " dz="
                              << client_message.input_frame()
                                     .move()
                                     .world_displacement_m()
                                     .z()
                              << std::endl;
                    const auto batch = world_->ApplyInput(
                        grant->session_id,
                        client_message.input_frame(),
                        heartbeat_interval_ms_
                    );
                    if (batch.has_value())
                    {
                        grpcmmo::session::v1::ServerMessage replication_message;
                        *replication_message.mutable_replication() = *batch;
                        stream->Write(replication_message);
                    }
                    continue;
                }

                if (client_message.has_chat())
                {
                    std::cout << "[game] chat session=" << grant->session_id
                              << " text='" << client_message.chat().text()
                              << "'" << std::endl;
                    grpcmmo::session::v1::ServerMessage chat_message;
                    auto* chat = chat_message.mutable_chat();
                    chat->set_message_id(client_message.chat().message_id());
                    chat->set_server_time_ms(grpcmmo::shared::NowMs());
                    chat->set_scope(client_message.chat().scope());
                    chat->set_sender_entity_id("entity-" + grant->character_id);
                    chat->set_sender_name(grant->character_name);
                    chat->set_target_id(client_message.chat().target_id());
                    chat->set_zone_id(grant->zone_id);
                    chat->set_text(client_message.chat().text());
                    stream->Write(chat_message);
                    continue;
                }

                if (client_message.has_ping())
                {
                    std::cout << "[game] ping session=" << grant->session_id
                              << " nonce=" << client_message.ping().nonce()
                              << std::endl;
                    grpcmmo::session::v1::ServerMessage pong_message;
                    auto* pong = pong_message.mutable_pong();
                    pong->set_nonce(client_message.ping().nonce());
                    pong->set_server_time_ms(grpcmmo::shared::NowMs());
                    stream->Write(pong_message);
                    continue;
                }
            }

            world_->DisconnectPlayer(grant->session_id);
            std::cout << "[game] session_closed session=" << grant->session_id
                      << std::endl;
            return grpc::Status::OK;
        }

      private:
        grpcmmo::storage::SqliteStorage* storage_;
        grpcmmo::game::AuthoritativeWorld* world_;
        std::uint64_t snapshot_send_interval_ms_;
        std::uint64_t interpolation_back_time_ms_;
        std::uint64_t heartbeat_interval_ms_;
    };
} // namespace

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);

    grpcmmo::storage::BackendConfig config;
    config.kind = grpcmmo::storage::BackendKind::kSqlite;
    config.connection_string = absl::GetFlag(FLAGS_db_path);

    grpcmmo::storage::SqliteStorage storage(config);
    storage.Initialize();

    grpcmmo::game::AuthoritativeWorld world;
    SessionServiceImpl service(
        &storage,
        &world,
        absl::GetFlag(FLAGS_snapshot_send_interval_ms),
        absl::GetFlag(FLAGS_interpolation_back_time_ms),
        absl::GetFlag(FLAGS_heartbeat_interval_ms)
    );

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        absl::GetFlag(FLAGS_listen_address), grpc::InsecureServerCredentials()
    );
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server)
    {
        std::cerr << "Failed to start game server." << std::endl;
        return 1;
    }

    std::cout << "grpcMMO game server listening on "
              << absl::GetFlag(FLAGS_listen_address) << " using "
              << storage.Describe() << std::endl;
    server->Wait();
    return 0;
}
