#pragma once

#include <chrono>
#include <cstdint>

namespace grpcmmo::shared
{
inline std::uint64_t NowMs()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        )
            .count()
    );
}
} // namespace grpcmmo::shared
