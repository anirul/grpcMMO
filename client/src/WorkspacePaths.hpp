#pragma once

#include <filesystem>

namespace grpcmmo::client
{
[[nodiscard]] std::filesystem::path NormalizePath(
    const std::filesystem::path& path
);
[[nodiscard]] std::filesystem::path ResolveProjectRoot();
} // namespace grpcmmo::client
