#pragma once

#include "PlanetConstants.hpp"
#include "PlanetMath.hpp"

namespace grpcmmo::shared::planet
{
struct PreviewPatchConfig
{
    const char* planet_id = "mars";
    const char* patch_id = "patch-000";
    double center_lat_deg = -14.0;
    double center_lon_deg = -65.0;
    double planet_radius_m = kMarsRadiusAtScale1To200M;
    double height_scale = 1.0 / kMarsScaleRatio1To200;
    double source_origin_height_m = 4058.711728;
    double spawn_altitude_m = 0.0;
};

[[nodiscard]] inline double PreviewPatchOriginHeightM(
    const PreviewPatchConfig& config)
{
    return config.source_origin_height_m * config.height_scale;
}

[[nodiscard]] inline double PreviewPatchSurfaceAltitudeM(
    const PreviewPatchConfig& config)
{
    return PreviewPatchOriginHeightM(config) + config.spawn_altitude_m;
}

[[nodiscard]] inline glm::dvec3 BuildPreviewPatchUp(
    const PreviewPatchConfig& config)
{
    return DirectionFromLatLonDegrees(
        config.center_lat_deg, config.center_lon_deg);
}

[[nodiscard]] inline TangentFrame BuildPreviewPatchFrame(
    const PreviewPatchConfig& config)
{
    return BuildTangentFrameFromUp(BuildPreviewPatchUp(config));
}

[[nodiscard]] inline glm::dvec3 BuildPreviewPatchOriginPlanetPosition(
    const PreviewPatchConfig& config)
{
    return PositionFromLatLonAltitude(
        config.planet_radius_m,
        config.center_lat_deg,
        config.center_lon_deg,
        PreviewPatchOriginHeightM(config));
}

inline constexpr PreviewPatchConfig kMarsPreviewPatch000{};
} // namespace grpcmmo::shared::planet
