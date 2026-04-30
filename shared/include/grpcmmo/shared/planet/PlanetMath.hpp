#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace grpcmmo::shared::planet
{
struct HorizonDistances
{
    double line_of_sight_m = 0.0;
    double surface_arc_m = 0.0;
};

struct TangentFrame
{
    glm::dvec3 east = glm::dvec3(1.0, 0.0, 0.0);
    glm::dvec3 north = glm::dvec3(0.0, 0.0, 1.0);
    glm::dvec3 up = glm::dvec3(0.0, 1.0, 0.0);
};

[[nodiscard]] inline glm::dvec3 NormalizeOrFallback(
    const glm::dvec3& value, const glm::dvec3& fallback)
{
    const double length = glm::length(value);
    if (length <= std::numeric_limits<double>::epsilon())
    {
        return fallback;
    }
    return value / length;
}

[[nodiscard]] inline glm::dvec3 SurfaceUpFromPosition(
    const glm::dvec3& planet_centered_position_m)
{
    return NormalizeOrFallback(
        planet_centered_position_m, glm::dvec3(0.0, 1.0, 0.0));
}

[[nodiscard]] inline glm::dvec3 GravityDirectionFromPosition(
    const glm::dvec3& planet_centered_position_m)
{
    return -SurfaceUpFromPosition(planet_centered_position_m);
}

[[nodiscard]] inline double AltitudeFromPosition(
    const glm::dvec3& planet_centered_position_m, double planet_radius_m)
{
    return glm::length(planet_centered_position_m) - planet_radius_m;
}

[[nodiscard]] inline glm::dvec3 ProjectToAltitude(
    const glm::dvec3& planet_centered_position_m,
    double planet_radius_m,
    double altitude_m)
{
    return SurfaceUpFromPosition(planet_centered_position_m) *
           (planet_radius_m + altitude_m);
}

[[nodiscard]] inline TangentFrame BuildTangentFrameFromUp(
    const glm::dvec3& up_unit)
{
    const glm::dvec3 up =
        NormalizeOrFallback(up_unit, glm::dvec3(0.0, 1.0, 0.0));
    const glm::dvec3 reference_axis =
        std::abs(glm::dot(up, glm::dvec3(0.0, 0.0, 1.0))) >= 0.999
            ? glm::dvec3(1.0, 0.0, 0.0)
            : glm::dvec3(0.0, 0.0, 1.0);

    TangentFrame frame;
    frame.up = up;
    frame.east = NormalizeOrFallback(
        glm::cross(reference_axis, up), glm::dvec3(1.0, 0.0, 0.0));
    frame.north = NormalizeOrFallback(
        glm::cross(up, frame.east), glm::dvec3(0.0, 0.0, 1.0));
    return frame;
}

[[nodiscard]] inline glm::dvec3 ProjectVectorOntoTangent(
    const glm::dvec3& vector, const glm::dvec3& up_unit)
{
    const glm::dvec3 up =
        NormalizeOrFallback(up_unit, glm::dvec3(0.0, 1.0, 0.0));
    return vector - (glm::dot(vector, up) * up);
}

[[nodiscard]] inline glm::dvec3 ProjectDirectionOntoTangent(
    const glm::dvec3& direction,
    const glm::dvec3& up_unit,
    const glm::dvec3& fallback)
{
    const glm::dvec3 tangent = ProjectVectorOntoTangent(direction, up_unit);
    return NormalizeOrFallback(tangent, fallback);
}

[[nodiscard]] inline glm::dvec3 TangentOffsetToPlanetSpace(
    const TangentFrame& frame, double east_offset_m, double north_offset_m)
{
    return (frame.east * east_offset_m) + (frame.north * north_offset_m);
}

[[nodiscard]] inline glm::dvec3 DirectionFromLatLonDegrees(
    double latitude_deg, double longitude_deg)
{
    const double latitude_radians = glm::radians(latitude_deg);
    const double longitude_radians = glm::radians(longitude_deg);
    const double cos_latitude = std::cos(latitude_radians);
    return glm::dvec3(
        std::cos(longitude_radians) * cos_latitude,
        std::sin(latitude_radians),
        std::sin(longitude_radians) * cos_latitude);
}

[[nodiscard]] inline glm::dvec3 PositionFromLatLonAltitude(
    double planet_radius_m,
    double latitude_deg,
    double longitude_deg,
    double altitude_m)
{
    return DirectionFromLatLonDegrees(latitude_deg, longitude_deg) *
           (planet_radius_m + altitude_m);
}

[[nodiscard]] inline glm::dvec3 WorldPositionToLocal(
    const glm::dvec3& planet_position_m,
    const glm::dvec3& local_origin_planet_position_m,
    const TangentFrame& frame)
{
    const glm::dvec3 offset =
        planet_position_m - local_origin_planet_position_m;
    return glm::dvec3(
        glm::dot(offset, frame.east),
        glm::dot(offset, frame.up),
        glm::dot(offset, frame.north));
}

[[nodiscard]] inline glm::dvec3 WorldDirectionToLocal(
    const glm::dvec3& world_direction, const TangentFrame& frame)
{
    return glm::dvec3(
        glm::dot(world_direction, frame.east),
        glm::dot(world_direction, frame.up),
        glm::dot(world_direction, frame.north));
}

[[nodiscard]] inline glm::dvec3 LocalDirectionToWorld(
    const glm::dvec3& local_direction, const TangentFrame& frame)
{
    return (frame.east * local_direction.x) + (frame.up * local_direction.y) +
           (frame.north * local_direction.z);
}

[[nodiscard]] inline HorizonDistances ComputeHorizonDistances(
    double planet_radius_m, double altitude_m)
{
    const double clamped_radius = std::max(planet_radius_m, 0.0);
    const double clamped_altitude = std::max(altitude_m, 0.0);

    HorizonDistances distances;
    distances.line_of_sight_m = std::sqrt(
        clamped_altitude * ((2.0 * clamped_radius) + clamped_altitude));

    if (clamped_radius <= std::numeric_limits<double>::epsilon())
    {
        distances.surface_arc_m = 0.0;
        return distances;
    }

    const double cosine = std::clamp(
        clamped_radius / (clamped_radius + clamped_altitude), -1.0, 1.0);
    distances.surface_arc_m = clamped_radius * std::acos(cosine);
    return distances;
}
} // namespace grpcmmo::shared::planet
