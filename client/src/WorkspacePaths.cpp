#include "WorkspacePaths.hpp"

#include <optional>
#include <system_error>
#include <vector>

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
                const DWORD length = GetModuleFileNameW(
                    nullptr, buffer.data(), static_cast<DWORD>(buffer.size())
                );
                if (length == 0)
                {
                    break;
                }
                if (length < buffer.size())
                {
                    return std::filesystem::path(
                        std::wstring(buffer.data(), length)
                    );
                }
                buffer.resize(buffer.size() * 2u);
            }
#else
            std::vector<char> buffer(1024, '\0');
            while (true)
            {
                const ssize_t length = readlink(
                    "/proc/self/exe", buffer.data(), buffer.size() - 1u
                );
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

        bool IsProjectRoot(const std::filesystem::path& path)
        {
            return std::filesystem::is_regular_file(path / "CMakeLists.txt") &&
                   std::filesystem::is_directory(path / "client") &&
                   std::filesystem::is_directory(path / "services");
        }

        std::optional<std::filesystem::path> FindProjectRootFrom(
            const std::filesystem::path& start
        )
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
    } // namespace

    std::filesystem::path NormalizePath(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        return error ? path.lexically_normal() : absolute.lexically_normal();
    }

    std::filesystem::path ResolveProjectRoot()
    {
        if (const auto from_current =
                FindProjectRootFrom(std::filesystem::current_path()))
        {
            return *from_current;
        }

        const auto executable_path = ResolveExecutablePath();
        if (const auto from_executable =
                FindProjectRootFrom(executable_path.parent_path()))
        {
            return *from_executable;
        }

        return NormalizePath(std::filesystem::current_path());
    }
} // namespace grpcmmo::client
