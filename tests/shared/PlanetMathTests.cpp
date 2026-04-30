#include <gtest/gtest.h>

#include <glm/geometric.hpp>

#include "grpcmmo/shared/planet/CubeSphereAddress.hpp"
#include "grpcmmo/shared/planet/PlanetConstants.hpp"
#include "grpcmmo/shared/planet/PlanetMath.hpp"

namespace grpcmmo::shared::planet
{
TEST(PlanetMathTests, SurfaceUpUsesPlanetCenteredDirection)
{
    const glm::dvec3 up = SurfaceUpFromPosition(glm::dvec3(10.0, 20.0, 0.0));
    EXPECT_NEAR(glm::length(up), 1.0, 1e-12);
    EXPECT_GT(up.y, up.x);
    EXPECT_NEAR(up.z, 0.0, 1e-12);
}

TEST(PlanetMathTests, ProjectToAltitudePreservesRequestedRadius)
{
    const glm::dvec3 projected =
        ProjectToAltitude(glm::dvec3(100.0, 100.0, 0.0), 1000.0, 25.0);
    EXPECT_NEAR(glm::length(projected), 1025.0, 1e-9);
    EXPECT_NEAR(AltitudeFromPosition(projected, 1000.0), 25.0, 1e-9);
}

TEST(PlanetMathTests, HorizonDistancesGrowWithAltitude)
{
    const HorizonDistances low = ComputeHorizonDistances(kMarsMeanRadiusM, 2.0);
    const HorizonDistances high =
        ComputeHorizonDistances(kMarsMeanRadiusM, 100.0);

    EXPECT_GT(low.line_of_sight_m, 3000.0);
    EXPECT_GT(low.surface_arc_m, 3000.0);
    EXPECT_GT(high.line_of_sight_m, low.line_of_sight_m);
    EXPECT_GT(high.surface_arc_m, low.surface_arc_m);
}

TEST(PlanetMathTests, MarsScaleOneToTwoHundredMatchesReferenceRadius)
{
    EXPECT_NEAR(kMarsRadiusAtScale1To200M, 16980.95, 1e-9);
    EXPECT_NEAR(
        ScaledRadiusMeters(kMarsMolaReferenceRadiusM, 200.0), 16980.95, 1e-9
    );
}

TEST(PlanetMathTests, CubeSphereChildrenSplitTileIntoQuadrants)
{
    const CubeSphereTileAddress root{CubeSphereFace::kPositiveY, 0u, 0u, 0u};
    ASSERT_TRUE(root.IsValid());

    const auto children = root.Children();
    EXPECT_EQ(children[0].lod, 1u);
    EXPECT_EQ(children[0].tile_x, 0u);
    EXPECT_EQ(children[0].tile_y, 0u);
    EXPECT_EQ(children[3].tile_x, 1u);
    EXPECT_EQ(children[3].tile_y, 1u);
    EXPECT_TRUE(children[3].IsValid());
}
} // namespace grpcmmo::shared::planet
