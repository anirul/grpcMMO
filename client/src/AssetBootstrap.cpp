#include "AssetBootstrap.hpp"

#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "frame/json/proto.h"
#include "frame/json/serialize_json.h"
#include "grpcmmo/shared/WorkspaceConfig.hpp"

#if defined(_WIN32) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace grpcmmo::client
{
namespace
{
std::filesystem::path ResolveExecutablePath()
{
#if defined(_WIN32) || defined(_WIN64)
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            break;
        }
        if (length < buffer.size())
        {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }
        buffer.resize(buffer.size() * 2u);
    }
#else
    std::vector<char> buffer(1024, '\0');
    while (true)
    {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1u);
        if (length < 0)
        {
            break;
        }
        if (static_cast<std::size_t>(length) < (buffer.size() - 1u))
        {
            buffer[static_cast<std::size_t>(length)] = '\0';
            return std::filesystem::path(buffer.data());
        }
        buffer.resize(buffer.size() * 2u, '\0');
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : absolute.lexically_normal();
}

bool IsProjectRoot(const std::filesystem::path& path)
{
    return std::filesystem::is_regular_file(path / "CMakeLists.txt") &&
           std::filesystem::is_directory(path / "client") &&
           std::filesystem::is_directory(path / "services");
}

std::optional<std::filesystem::path> FindProjectRootFrom(const std::filesystem::path& start)
{
    auto candidate = NormalizePath(start);
    while (true)
    {
        if (IsProjectRoot(candidate))
        {
            return candidate;
        }

        const auto parent = candidate.parent_path();
        if (parent.empty() || parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    return std::nullopt;
}

std::filesystem::path ResolveProjectRoot()
{
    if (const auto from_current = FindProjectRootFrom(std::filesystem::current_path()))
    {
        return *from_current;
    }

    const auto executable_path = ResolveExecutablePath();
    if (const auto from_executable = FindProjectRootFrom(executable_path.parent_path()))
    {
        return *from_executable;
    }

    return NormalizePath(std::filesystem::current_path());
}

void CopyFileIfChanged(const std::filesystem::path& source,
                       const std::filesystem::path& destination)
{
    std::ifstream in(source, std::ios::binary);
    if (!in.is_open())
    {
        throw std::runtime_error("failed to read " + source.string());
    }

    std::ostringstream source_buffer;
    source_buffer << in.rdbuf();
    const std::string source_data = source_buffer.str();

    bool should_write = true;
    std::ifstream existing(destination, std::ios::binary);
    if (existing.is_open())
    {
        std::ostringstream existing_buffer;
        existing_buffer << existing.rdbuf();
        should_write = existing_buffer.str() != source_data;
    }

    if (!should_write)
    {
        return;
    }

    std::filesystem::create_directories(destination.parent_path());
    std::ofstream out(destination, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        throw std::runtime_error("failed to write " + destination.string());
    }
    out.write(source_data.data(), static_cast<std::streamsize>(source_data.size()));
}
} // namespace

std::filesystem::path AssetBootstrap::EnsureFrameAssetsAvailable() const
{
    const std::filesystem::path project_root = ResolveProjectRoot();
    const std::filesystem::path frame_root = NormalizePath(grpcmmo::shared::kFrameRoot);
    const std::filesystem::path source_asset_root = frame_root / "asset";
    const std::filesystem::path destination_asset_root = project_root / "asset";

    const std::array<std::filesystem::path, 3> source_directories = {
        source_asset_root / "shader",
        source_asset_root / "cubemap",
        source_asset_root / "model"};

    for (const auto& source_directory : source_directories)
    {
        if (!std::filesystem::exists(source_directory))
        {
            throw std::runtime_error("Frame asset directory not found: " +
                                     source_directory.string());
        }

        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(source_directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const auto relative = std::filesystem::relative(entry.path(), source_asset_root);
            CopyFileIfChanged(entry.path(), destination_asset_root / relative);
        }
    }

    return destination_asset_root / "shader";
}

std::filesystem::path AssetBootstrap::WriteGeneratedLevelJson(
    const frame::proto::Level& level) const
{
    const std::filesystem::path project_root = ResolveProjectRoot();
    const std::filesystem::path output_path =
        project_root / "asset" / "generated" / "grpcmmo_third_person.json";
    std::filesystem::create_directories(output_path.parent_path());
    frame::json::SaveProtoToJsonFile(level, output_path);
    return output_path;
}
} // namespace grpcmmo::client
