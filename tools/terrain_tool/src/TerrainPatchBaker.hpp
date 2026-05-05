#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace grpcmmo::tools::terrain
{
    enum class RasterSampleFormat
    {
        kUnknown,
        kUnsignedInteger,
        kSignedInteger,
        kFloat
    };

    struct RasterMetadata
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint16_t bits_per_sample = 0;
        std::uint16_t samples_per_pixel = 0;
        RasterSampleFormat sample_format = RasterSampleFormat::kUnknown;
    };

    struct BakeSettings
    {
        std::filesystem::path input_tiff;
        std::filesystem::path output_dir;
        std::string planet_id = "mars";
        std::string patch_id = "patch-000";
        double center_lat_deg = 0.0;
        double center_lon_deg = 0.0;
        double lat_span_deg = 1.0;
        double lon_span_deg = 1.0;
        std::uint32_t output_rows = 129;
        std::uint32_t output_cols = 129;
        double planet_radius_m = 3396190.0;
        double height_scale = 1.0;
        double obj_vertical_scale = 1.0;
    };

    struct BakeResult
    {
        RasterMetadata source_metadata{};
        std::filesystem::path height_file;
        std::filesystem::path preview_gltf_file;
        std::filesystem::path preview_texture_file;
        std::filesystem::path metadata_file;
        std::vector<float> relative_heights_m{};
        double min_lat_deg = 0.0;
        double max_lat_deg = 0.0;
        double min_lon_deg = 0.0;
        double max_lon_deg = 0.0;
        double origin_height_m = 0.0;
        float min_relative_height_m = 0.0f;
        float max_relative_height_m = 0.0f;
        std::size_t vertex_count = 0;
        std::size_t triangle_count = 0;
    };

    [[nodiscard]] RasterMetadata InspectRaster(
        const std::filesystem::path& input_tiff
    );
    [[nodiscard]] BakeResult BakeTerrainPatch(const BakeSettings& settings);
    [[nodiscard]] const char* RasterSampleFormatName(
        RasterSampleFormat sample_format
    );
} // namespace grpcmmo::tools::terrain
