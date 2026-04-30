#pragma once

#include <array>
#include <cstdint>

namespace grpcmmo::shared::planet
{
enum class CubeSphereFace : std::uint8_t
{
    kPositiveX = 0,
    kNegativeX = 1,
    kPositiveY = 2,
    kNegativeY = 3,
    kPositiveZ = 4,
    kNegativeZ = 5
};

struct CubeSphereTileAddress
{
    CubeSphereFace face = CubeSphereFace::kPositiveY;
    std::uint32_t lod = 0;
    std::uint32_t tile_x = 0;
    std::uint32_t tile_y = 0;

    [[nodiscard]] std::uint32_t ResolutionPerFace() const
    {
        return 1u << lod;
    }

    [[nodiscard]] bool IsValid() const
    {
        const std::uint32_t resolution = ResolutionPerFace();
        return tile_x < resolution && tile_y < resolution;
    }

    [[nodiscard]] std::array<CubeSphereTileAddress, 4> Children() const
    {
        return {{
            CubeSphereTileAddress{face, lod + 1u, tile_x * 2u, tile_y * 2u},
            CubeSphereTileAddress{
                face, lod + 1u, tile_x * 2u + 1u, tile_y * 2u
            },
            CubeSphereTileAddress{
                face, lod + 1u, tile_x * 2u, tile_y * 2u + 1u
            },
            CubeSphereTileAddress{
                face, lod + 1u, tile_x * 2u + 1u, tile_y * 2u + 1u
            },
        }};
    }
};
} // namespace grpcmmo::shared::planet
