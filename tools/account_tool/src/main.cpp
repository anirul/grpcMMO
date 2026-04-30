#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "grpcmmo/storage/SqliteStorage.hpp"

ABSL_FLAG(
    std::string, db_path, "data/grpcmmo.sqlite3", "SQLite database path.");
ABSL_FLAG(std::string, login_name, "", "Login name for the new account.");
ABSL_FLAG(std::string, password, "", "Password for the new account.");
ABSL_FLAG(std::string, display_name, "", "Display name for the new account.");

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);

    const std::string login_name = absl::GetFlag(FLAGS_login_name);
    const std::string password = absl::GetFlag(FLAGS_password);
    std::string display_name = absl::GetFlag(FLAGS_display_name);

    if (login_name.empty() || password.empty())
    {
        std::cerr << "Both --login_name and --password are required."
                  << std::endl;
        return 1;
    }

    if (display_name.empty())
    {
        display_name = login_name;
    }

    grpcmmo::storage::BackendConfig config;
    config.kind = grpcmmo::storage::BackendKind::kSqlite;
    config.connection_string = absl::GetFlag(FLAGS_db_path);

    grpcmmo::storage::SqliteStorage storage(config);
    storage.Initialize();

    std::string error_message;
    const auto account = storage.CreateAccount(
        login_name, password, display_name, &error_message);
    if (!account.has_value())
    {
        std::cerr << "CreateAccount failed: " << error_message << std::endl;
        return 1;
    }

    std::cout << "Created account" << std::endl;
    std::cout << "  account_id: " << account->account_id << std::endl;
    std::cout << "  login_name: " << login_name << std::endl;
    std::cout << "  display_name: " << account->display_name << std::endl;
    return 0;
}
