#pragma once

namespace grpcmmo::shared::planet
{
    inline constexpr double kMarsMeanRadiusM = 3389500.0;
    inline constexpr double kMarsMolaReferenceRadiusM = 3396190.0;
    inline constexpr double kMarsScaleRatio1To100 = 100.0;
    inline constexpr double kMarsScaleRatio1To200 = 200.0;
    inline constexpr double kMarsRadiusAtScale1To100M =
        kMarsMolaReferenceRadiusM / kMarsScaleRatio1To100;
    inline constexpr double kMarsRadiusAtScale1To200M =
        kMarsMolaReferenceRadiusM / kMarsScaleRatio1To200;

    [[nodiscard]] constexpr double ScaledRadiusMeters(
        double base_radius_m, double scale_ratio
    )
    {
        return scale_ratio > 0.0 ? (base_radius_m / scale_ratio)
                                 : base_radius_m;
    }
} // namespace grpcmmo::shared::planet
