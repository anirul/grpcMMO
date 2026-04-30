#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "grpcmmo/storage/StorageBackend.hpp"

struct sqlite3;

namespace grpcmmo::storage
{
    struct AccountRecord
    {
        std::string account_id;
        std::string display_name;
    };

    struct CharacterRecord
    {
        std::string character_id;
        std::string account_id;
        std::string realm_id;
        std::string name;
        std::string planet_id;
        std::string zone_id;
        std::uint64_t last_seen_time_ms = 0;
        bool online = false;
    };

    struct SessionGrantRecord
    {
        std::string session_id;
        std::string session_token;
        std::string account_id;
        std::string character_id;
        std::string character_name;
        std::string realm_id;
        std::string planet_id;
        std::string zone_id;
        std::uint64_t expires_at_ms = 0;
    };

    class SqliteStorage final : public StorageBackend
    {
      public:
        explicit SqliteStorage(BackendConfig config = {});
        ~SqliteStorage() override;

        SqliteStorage(const SqliteStorage&) = delete;
        SqliteStorage& operator=(const SqliteStorage&) = delete;

        void Initialize();

        BackendKind Kind() const override;
        std::string Describe() const override;

        std::optional<AccountRecord> CreateAccount(
            const std::string& login_name,
            const std::string& password,
            const std::string& display_name,
            std::string* error_message = nullptr
        );
        std::optional<AccountRecord> Login(
            const std::string& login_name, const std::string& password
        );
        std::vector<CharacterRecord> ListCharacters(
            const std::string& account_access_token, const std::string& realm_id
        );
        std::optional<CharacterRecord> CreateCharacter(
            const std::string& account_access_token,
            const std::string& realm_id,
            const std::string& name,
            std::string* error_message = nullptr
        );
        std::optional<SessionGrantRecord> CreateSessionGrant(
            const std::string& account_access_token,
            const std::string& realm_id,
            const std::string& character_id,
            std::string* error_message = nullptr
        );
        std::optional<SessionGrantRecord> FindSessionGrant(
            const std::string& session_token
        );

        static std::string MakeAccountAccessToken(const std::string& account_id
        );

      private:
        std::optional<std::string> AccountIdFromAccessToken(
            const std::string& account_access_token
        ) const;
        void EnsureOpen();
        void EnsureSchema();
        void EnsureSeedData();
        std::string MakeId(const std::string& prefix);
        std::optional<CharacterRecord> FindCharacterById(
            const std::string& account_id,
            const std::string& realm_id,
            const std::string& character_id
        );

        BackendConfig config_;
        sqlite3* db_ = nullptr;
        mutable std::mutex mutex_;
        std::uint64_t next_local_id_ = 1;
    };
} // namespace grpcmmo::storage
