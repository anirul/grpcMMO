#include <grpcpp/grpcpp.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "auth/v1/auth.grpc.pb.h"
#include "grpcmmo/shared/Time.hpp"
#include "grpcmmo/storage/SqliteStorage.hpp"

ABSL_FLAG(
    std::string,
    listen_address,
    "0.0.0.0:50050",
    "Auth server listen address.");
ABSL_FLAG(
    std::string, db_path, "data/grpcmmo.sqlite3", "SQLite database path.");
ABSL_FLAG(std::string, realm_id, "realm-dev", "Realm identifier.");
ABSL_FLAG(std::string, realm_name, "Development Realm", "Realm display name.");
ABSL_FLAG(
    std::string,
    game_host,
    "127.0.0.1",
    "Game server host advertised by auth.");
ABSL_FLAG(int, game_port, 50051, "Game server port advertised by auth.");

namespace
{
class AuthServiceImpl final : public grpcmmo::auth::v1::AuthService::Service
{
  public:
    AuthServiceImpl(
        grpcmmo::storage::SqliteStorage* storage,
        std::string realm_id,
        std::string realm_name,
        std::string game_host,
        std::uint32_t game_port)
        : storage_(storage), realm_id_(std::move(realm_id)),
          realm_name_(std::move(realm_name)), game_host_(std::move(game_host)),
          game_port_(game_port)
    {
    }

    grpc::Status Login(
        grpc::ServerContext*,
        const grpcmmo::auth::v1::LoginRequest* request,
        grpcmmo::auth::v1::LoginResponse* response) override
    {
        const auto account =
            storage_->Login(request->login_name(), request->password());
        if (!account.has_value())
        {
            std::cout << "[auth] login failed for '" << request->login_name()
                      << "'" << std::endl;
            return grpc::Status(
                grpc::StatusCode::UNAUTHENTICATED, "invalid login credentials");
        }

        auto* account_summary = response->mutable_account();
        account_summary->set_account_id(account->account_id);
        account_summary->set_display_name(account->display_name);
        response->set_account_access_token(
            grpcmmo::storage::SqliteStorage::MakeAccountAccessToken(
                account->account_id));
        response->set_expires_at_ms(
            grpcmmo::shared::NowMs() + (24ULL * 60ULL * 60ULL * 1000ULL));

        auto* realm = response->add_realms();
        realm->set_realm_id(realm_id_);
        realm->set_display_name(realm_name_);
        realm->set_host(game_host_);
        realm->set_port(game_port_);
        realm->set_tls_required(false);
        std::cout << "[auth] login ok account=" << account->account_id
                  << " login_name='" << request->login_name() << "'"
                  << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status ListCharacters(
        grpc::ServerContext*,
        const grpcmmo::auth::v1::ListCharactersRequest* request,
        grpcmmo::auth::v1::ListCharactersResponse* response) override
    {
        const auto characters = storage_->ListCharacters(
            request->account_access_token(), request->realm_id());
        for (const auto& character : characters)
        {
            auto* item = response->add_characters();
            item->set_character_id(character.character_id);
            item->set_name(character.name);
            item->set_planet_id(character.planet_id);
            item->set_zone_id(character.zone_id);
            item->set_last_seen_time_ms(character.last_seen_time_ms);
            item->set_online(character.online);
        }
        std::cout << "[auth] list_characters realm=" << request->realm_id()
                  << " count=" << characters.size() << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status CreateCharacter(
        grpc::ServerContext*,
        const grpcmmo::auth::v1::CreateCharacterRequest* request,
        grpcmmo::auth::v1::CreateCharacterResponse* response) override
    {
        std::string error_message;
        const auto character = storage_->CreateCharacter(
            request->account_access_token(),
            request->realm_id(),
            request->name(),
            &error_message);
        if (!character.has_value())
        {
            std::cout << "[auth] create_character failed name='"
                      << request->name() << "' reason=" << error_message
                      << std::endl;
            return grpc::Status(
                grpc::StatusCode::ALREADY_EXISTS, error_message);
        }

        auto* created = response->mutable_character();
        created->set_character_id(character->character_id);
        created->set_name(character->name);
        created->set_planet_id(character->planet_id);
        created->set_zone_id(character->zone_id);
        created->set_last_seen_time_ms(character->last_seen_time_ms);
        created->set_online(character->online);
        std::cout << "[auth] create_character ok character="
                  << character->character_id << " name='" << character->name
                  << "'" << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status CreateSessionGrant(
        grpc::ServerContext*,
        const grpcmmo::auth::v1::CreateSessionGrantRequest* request,
        grpcmmo::auth::v1::CreateSessionGrantResponse* response) override
    {
        std::string error_message;
        const auto grant = storage_->CreateSessionGrant(
            request->account_access_token(),
            request->realm_id(),
            request->character_id(),
            &error_message);
        if (!grant.has_value())
        {
            std::cout << "[auth] create_session_grant failed character="
                      << request->character_id() << " reason=" << error_message
                      << std::endl;
            return grpc::Status(
                grpc::StatusCode::FAILED_PRECONDITION, error_message);
        }

        response->set_session_id(grant->session_id);
        response->set_session_token(grant->session_token);
        response->set_expires_at_ms(grant->expires_at_ms);

        auto* endpoint = response->mutable_endpoint();
        endpoint->set_realm_id(realm_id_);
        endpoint->set_display_name(realm_name_);
        endpoint->set_host(game_host_);
        endpoint->set_port(game_port_);
        endpoint->set_tls_required(false);

        auto* character = response->mutable_character();
        character->set_character_id(grant->character_id);
        character->set_name(grant->character_name);
        character->set_planet_id(grant->planet_id);
        character->set_zone_id(grant->zone_id);
        character->set_last_seen_time_ms(grpcmmo::shared::NowMs());
        character->set_online(false);
        std::cout << "[auth] create_session_grant ok session="
                  << grant->session_id << " character=" << grant->character_id
                  << std::endl;
        return grpc::Status::OK;
    }

  private:
    grpcmmo::storage::SqliteStorage* storage_;
    std::string realm_id_;
    std::string realm_name_;
    std::string game_host_;
    std::uint32_t game_port_;
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

    AuthServiceImpl service(
        &storage,
        absl::GetFlag(FLAGS_realm_id),
        absl::GetFlag(FLAGS_realm_name),
        absl::GetFlag(FLAGS_game_host),
        static_cast<std::uint32_t>(absl::GetFlag(FLAGS_game_port)));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        absl::GetFlag(FLAGS_listen_address), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server)
    {
        std::cerr << "Failed to start auth server." << std::endl;
        return 1;
    }

    std::cout << "grpcMMO auth server listening on "
              << absl::GetFlag(FLAGS_listen_address) << " using "
              << storage.Describe() << std::endl;
    server->Wait();
    return 0;
}
