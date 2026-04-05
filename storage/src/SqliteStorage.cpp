#include "grpcmmo/storage/SqliteStorage.hpp"

#include <filesystem>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#include "grpcmmo/shared/Time.hpp"

namespace grpcmmo::storage
{
namespace
{
class Statement final
{
public:
    Statement(sqlite3* db, const char* sql)
    {
        sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr);
    }

    ~Statement()
    {
        if (statement_ != nullptr)
        {
            sqlite3_finalize(statement_);
        }
    }

    sqlite3_stmt* Get() const
    {
        return statement_;
    }

private:
    sqlite3_stmt* statement_ = nullptr;
};

std::string ColumnText(sqlite3_stmt* statement, int column)
{
    const unsigned char* value = sqlite3_column_text(statement, column);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

void BindText(sqlite3_stmt* statement, int index, const std::string& value)
{
    sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Exec(sqlite3* db, const char* sql)
{
    char* error_message = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &error_message);
    if (error_message != nullptr)
    {
        sqlite3_free(error_message);
    }
}
} // namespace

SqliteStorage::SqliteStorage(BackendConfig config)
    : config_(std::move(config))
{
    if (config_.connection_string.empty())
    {
        config_.connection_string = "data/grpcmmo.sqlite3";
    }
}

SqliteStorage::~SqliteStorage()
{
    std::scoped_lock lock(mutex_);
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SqliteStorage::Initialize()
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();
    EnsureSchema();
    EnsureSeedData();
}

BackendKind SqliteStorage::Kind() const
{
    return BackendKind::kSqlite;
}

std::string SqliteStorage::Describe() const
{
    return "SQLite backend at " + config_.connection_string;
}

std::optional<AccountRecord> SqliteStorage::CreateAccount(
    const std::string& login_name, const std::string& password,
    const std::string& display_name, std::string* error_message)
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();

    const std::string account_id = MakeId("acct");

    Statement statement(
        db_,
        "INSERT INTO accounts(account_id, login_name, password, display_name) "
        "VALUES(?1, ?2, ?3, ?4);");
    BindText(statement.Get(), 1, account_id);
    BindText(statement.Get(), 2, login_name);
    BindText(statement.Get(), 3, password);
    BindText(statement.Get(), 4, display_name);

    if (sqlite3_step(statement.Get()) != SQLITE_DONE)
    {
        if (error_message != nullptr)
        {
            *error_message = "login name already exists";
        }
        return std::nullopt;
    }

    AccountRecord record;
    record.account_id = account_id;
    record.display_name = display_name;
    return record;
}

std::optional<AccountRecord> SqliteStorage::Login(const std::string& login_name,
                                                  const std::string& password)
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();

    Statement statement(db_,
                        "SELECT account_id, display_name "
                        "FROM accounts WHERE login_name = ?1 AND password = ?2;");
    BindText(statement.Get(), 1, login_name);
    BindText(statement.Get(), 2, password);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    AccountRecord record;
    record.account_id = ColumnText(statement.Get(), 0);
    record.display_name = ColumnText(statement.Get(), 1);
    return record;
}

std::vector<CharacterRecord> SqliteStorage::ListCharacters(
    const std::string& account_access_token, const std::string& realm_id)
{
    std::vector<CharacterRecord> records;

    std::scoped_lock lock(mutex_);
    EnsureOpen();

    const auto account_id = AccountIdFromAccessToken(account_access_token);
    if (!account_id.has_value())
    {
        return records;
    }

    Statement statement(
        db_,
        "SELECT character_id, account_id, realm_id, name, planet_id, zone_id, "
        "last_seen_time_ms, online "
        "FROM characters WHERE account_id = ?1 AND realm_id = ?2 ORDER BY name;");
    BindText(statement.Get(), 1, *account_id);
    BindText(statement.Get(), 2, realm_id);

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        CharacterRecord record;
        record.character_id = ColumnText(statement.Get(), 0);
        record.account_id = ColumnText(statement.Get(), 1);
        record.realm_id = ColumnText(statement.Get(), 2);
        record.name = ColumnText(statement.Get(), 3);
        record.planet_id = ColumnText(statement.Get(), 4);
        record.zone_id = ColumnText(statement.Get(), 5);
        record.last_seen_time_ms =
            static_cast<std::uint64_t>(sqlite3_column_int64(statement.Get(), 6));
        record.online = sqlite3_column_int(statement.Get(), 7) != 0;
        records.push_back(std::move(record));
    }

    return records;
}

std::optional<CharacterRecord> SqliteStorage::CreateCharacter(
    const std::string& account_access_token, const std::string& realm_id,
    const std::string& name, std::string* error_message)
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();

    const auto account_id = AccountIdFromAccessToken(account_access_token);
    if (!account_id.has_value())
    {
        if (error_message != nullptr)
        {
            *error_message = "invalid account access token";
        }
        return std::nullopt;
    }

    const std::string character_id = MakeId("char");
    const std::uint64_t now_ms = grpcmmo::shared::NowMs();

    Statement statement(
        db_,
        "INSERT INTO characters(character_id, account_id, realm_id, name, planet_id, "
        "zone_id, last_seen_time_ms, online) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, 0);");
    BindText(statement.Get(), 1, character_id);
    BindText(statement.Get(), 2, *account_id);
    BindText(statement.Get(), 3, realm_id);
    BindText(statement.Get(), 4, name);
    BindText(statement.Get(), 5, "mars-dev");
    BindText(statement.Get(), 6, "zone-001");
    sqlite3_bind_int64(statement.Get(), 7, static_cast<sqlite3_int64>(now_ms));

    const int rc = sqlite3_step(statement.Get());
    if (rc != SQLITE_DONE)
    {
        if (error_message != nullptr)
        {
            *error_message = "character name already exists for that realm";
        }
        return std::nullopt;
    }

    return FindCharacterById(*account_id, realm_id, character_id);
}

std::optional<SessionGrantRecord> SqliteStorage::CreateSessionGrant(
    const std::string& account_access_token, const std::string& realm_id,
    const std::string& character_id, std::string* error_message)
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();

    const auto account_id = AccountIdFromAccessToken(account_access_token);
    if (!account_id.has_value())
    {
        if (error_message != nullptr)
        {
            *error_message = "invalid account access token";
        }
        return std::nullopt;
    }

    const auto character = FindCharacterById(*account_id, realm_id, character_id);
    if (!character.has_value())
    {
        if (error_message != nullptr)
        {
            *error_message = "character not found";
        }
        return std::nullopt;
    }

    const std::string session_id = MakeId("session");
    const std::string session_token = MakeId("token");
    const std::uint64_t expires_at_ms =
        grpcmmo::shared::NowMs() + (60ULL * 60ULL * 1000ULL);

    Statement statement(
        db_,
        "INSERT INTO session_grants(session_id, session_token, account_id, character_id, "
        "realm_id, planet_id, zone_id, expires_at_ms) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);");
    BindText(statement.Get(), 1, session_id);
    BindText(statement.Get(), 2, session_token);
    BindText(statement.Get(), 3, *account_id);
    BindText(statement.Get(), 4, character->character_id);
    BindText(statement.Get(), 5, character->realm_id);
    BindText(statement.Get(), 6, character->planet_id);
    BindText(statement.Get(), 7, character->zone_id);
    sqlite3_bind_int64(statement.Get(), 8,
                       static_cast<sqlite3_int64>(expires_at_ms));

    if (sqlite3_step(statement.Get()) != SQLITE_DONE)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to create session grant";
        }
        return std::nullopt;
    }

    SessionGrantRecord record;
    record.session_id = session_id;
    record.session_token = session_token;
    record.account_id = *account_id;
    record.character_id = character->character_id;
    record.character_name = character->name;
    record.realm_id = character->realm_id;
    record.planet_id = character->planet_id;
    record.zone_id = character->zone_id;
    record.expires_at_ms = expires_at_ms;
    return record;
}

std::optional<SessionGrantRecord> SqliteStorage::FindSessionGrant(
    const std::string& session_token)
{
    std::scoped_lock lock(mutex_);
    EnsureOpen();

    Statement statement(
        db_,
        "SELECT sg.session_id, sg.session_token, sg.account_id, sg.character_id, "
        "c.name, sg.realm_id, sg.planet_id, sg.zone_id, sg.expires_at_ms "
        "FROM session_grants sg "
        "JOIN characters c ON c.character_id = sg.character_id "
        "WHERE sg.session_token = ?1;");
    BindText(statement.Get(), 1, session_token);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    SessionGrantRecord record;
    record.session_id = ColumnText(statement.Get(), 0);
    record.session_token = ColumnText(statement.Get(), 1);
    record.account_id = ColumnText(statement.Get(), 2);
    record.character_id = ColumnText(statement.Get(), 3);
    record.character_name = ColumnText(statement.Get(), 4);
    record.realm_id = ColumnText(statement.Get(), 5);
    record.planet_id = ColumnText(statement.Get(), 6);
    record.zone_id = ColumnText(statement.Get(), 7);
    record.expires_at_ms =
        static_cast<std::uint64_t>(sqlite3_column_int64(statement.Get(), 8));

    if (record.expires_at_ms < grpcmmo::shared::NowMs())
    {
        return std::nullopt;
    }

    return record;
}

std::string SqliteStorage::MakeAccountAccessToken(const std::string& account_id)
{
    return "acct:" + account_id;
}

std::optional<std::string> SqliteStorage::AccountIdFromAccessToken(
    const std::string& account_access_token) const
{
    constexpr const char* kPrefix = "acct:";
    if (account_access_token.rfind(kPrefix, 0) != 0)
    {
        return std::nullopt;
    }
    const std::string account_id = account_access_token.substr(5);
    return account_id.empty() ? std::nullopt : std::optional<std::string>(account_id);
}

void SqliteStorage::EnsureOpen()
{
    if (db_ != nullptr)
    {
        return;
    }

    const std::filesystem::path db_path(config_.connection_string);
    if (!db_path.parent_path().empty())
    {
        std::filesystem::create_directories(db_path.parent_path());
    }

    sqlite3_open_v2(config_.connection_string.c_str(), &db_,
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                    nullptr);
}

void SqliteStorage::EnsureSchema()
{
    Exec(db_,
         "CREATE TABLE IF NOT EXISTS accounts ("
         "account_id TEXT PRIMARY KEY,"
         "login_name TEXT NOT NULL UNIQUE,"
         "password TEXT NOT NULL,"
         "display_name TEXT NOT NULL);");
    Exec(db_,
         "CREATE TABLE IF NOT EXISTS characters ("
         "character_id TEXT PRIMARY KEY,"
         "account_id TEXT NOT NULL,"
         "realm_id TEXT NOT NULL,"
         "name TEXT NOT NULL,"
         "planet_id TEXT NOT NULL,"
         "zone_id TEXT NOT NULL,"
         "last_seen_time_ms INTEGER NOT NULL,"
         "online INTEGER NOT NULL DEFAULT 0,"
         "UNIQUE(realm_id, name));");
    Exec(db_,
         "CREATE TABLE IF NOT EXISTS session_grants ("
         "session_id TEXT PRIMARY KEY,"
         "session_token TEXT NOT NULL UNIQUE,"
         "account_id TEXT NOT NULL,"
         "character_id TEXT NOT NULL,"
         "realm_id TEXT NOT NULL,"
         "planet_id TEXT NOT NULL,"
         "zone_id TEXT NOT NULL,"
         "expires_at_ms INTEGER NOT NULL);");
}

void SqliteStorage::EnsureSeedData()
{
    Exec(db_,
         "INSERT OR IGNORE INTO accounts(account_id, login_name, password, display_name) "
         "VALUES('acct-demo', 'demo', 'demo', 'Demo Account');");
}

std::string SqliteStorage::MakeId(const std::string& prefix)
{
    std::ostringstream stream;
    stream << prefix << '-' << grpcmmo::shared::NowMs() << '-' << next_local_id_++;
    return stream.str();
}

std::optional<CharacterRecord> SqliteStorage::FindCharacterById(
    const std::string& account_id, const std::string& realm_id,
    const std::string& character_id)
{
    Statement statement(
        db_,
        "SELECT character_id, account_id, realm_id, name, planet_id, zone_id, "
        "last_seen_time_ms, online "
        "FROM characters WHERE account_id = ?1 AND realm_id = ?2 AND character_id = ?3;");
    BindText(statement.Get(), 1, account_id);
    BindText(statement.Get(), 2, realm_id);
    BindText(statement.Get(), 3, character_id);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    CharacterRecord record;
    record.character_id = ColumnText(statement.Get(), 0);
    record.account_id = ColumnText(statement.Get(), 1);
    record.realm_id = ColumnText(statement.Get(), 2);
    record.name = ColumnText(statement.Get(), 3);
    record.planet_id = ColumnText(statement.Get(), 4);
    record.zone_id = ColumnText(statement.Get(), 5);
    record.last_seen_time_ms =
        static_cast<std::uint64_t>(sqlite3_column_int64(statement.Get(), 6));
    record.online = sqlite3_column_int(statement.Get(), 7) != 0;
    return record;
}
} // namespace grpcmmo::storage
