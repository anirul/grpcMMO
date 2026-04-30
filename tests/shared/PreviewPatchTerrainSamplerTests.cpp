#include <gtest/gtest.h>

#include "grpcmmo/shared/planet/PreviewPatchTerrainSampler.hpp"

namespace grpcmmo::shared::planet
{
TEST(PreviewPatchTerrainSamplerTests, SamplesFirstTrianglePlane)
{
    const double height_m =
        PreviewPatchTerrainSampler::SampleTriangleSplitCellHeight(
            0.0f, 10.0f, 20.0f, 30.0f, 0.25, 0.25
        );

    EXPECT_NEAR(height_m, 7.5, 1e-12);
}

TEST(PreviewPatchTerrainSamplerTests, SamplesSecondTrianglePlane)
{
    const double height_m =
        PreviewPatchTerrainSampler::SampleTriangleSplitCellHeight(
            0.0f, 10.0f, 20.0f, 30.0f, 0.75, 0.75
        );

    EXPECT_NEAR(height_m, 22.5, 1e-12);
}

TEST(PreviewPatchTerrainSamplerTests, MatchesTriangleSplitAlongCellDiagonal)
{
    const double height_m =
        PreviewPatchTerrainSampler::SampleTriangleSplitCellHeight(
            0.0f, 0.0f, 0.0f, 4.0f, 0.5, 0.5
        );

    EXPECT_NEAR(height_m, 0.0, 1e-12);
}
} // namespace grpcmmo::shared::planet
