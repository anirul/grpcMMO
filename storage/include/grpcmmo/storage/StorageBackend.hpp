#pragma once

#include <string>

namespace grpcmmo::storage
{
enum class BackendKind
{
    kSqlite,
    kPostgres,
    kRemoteService
};

struct BackendConfig
{
    BackendKind kind = BackendKind::kSqlite;
    std::string connection_string;
};

class StorageBackend
{
  public:
    virtual ~StorageBackend() = default;

    virtual BackendKind Kind() const = 0;
    virtual std::string Describe() const = 0;
};
} // namespace grpcmmo::storage
