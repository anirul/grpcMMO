#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "grpcmmo/shared/planet/PlanetConstants.hpp"
#include "grpcmmo/shared/WorkspaceConfig.hpp"

#include "TerrainPatchBaker.hpp"

namespace
{
const std::string kDefaultInputTiff =
    std::filesystem::path(grpcmmo::shared::kDataRoot)
        .append("sources")
        .append("mars")
        .append("mars_mgs_mola_dem_463m.tif")
        .string();

const std::string kDefaultOutputDir =
    std::filesystem::path(grpcmmo::shared::kDataRoot)
        .append("tiles")
        .append("mars")
        .append("patch-000")
        .string();
} // namespace

ABSL_FLAG(bool, inspect_only, false, "Print raster metadata and exit.");
ABSL_FLAG(std::string, input_tiff, kDefaultInputTiff,
          "Source single-band Mars TIFF to inspect or bake.");
ABSL_FLAG(std::string, output_dir, kDefaultOutputDir,
          "Directory for baked height data, patch metadata, and preview glTF.");
ABSL_FLAG(std::string, planet_id, "mars", "Planet identifier written into patch metadata.");
ABSL_FLAG(std::string, patch_id, "patch-000", "Patch identifier written into patch metadata.");
ABSL_FLAG(double, center_lat_deg, -14.0, "Center latitude for the baked patch.");
ABSL_FLAG(double, center_lon_deg, -65.0, "Center longitude for the baked patch.");
ABSL_FLAG(double, lat_span_deg, 10.0, "Latitude span in degrees for the baked patch.");
ABSL_FLAG(double, lon_span_deg, 10.0, "Longitude span in degrees for the baked patch.");
ABSL_FLAG(std::uint32_t, output_rows, 65, "Output grid row count.");
ABSL_FLAG(std::uint32_t, output_cols, 65, "Output grid column count.");
ABSL_FLAG(double, planet_radius_m,
          grpcmmo::shared::planet::kMarsRadiusAtScale1To200M,
          "Planet radius used for preview mesh coordinates.");
ABSL_FLAG(double, height_scale,
          1.0 / grpcmmo::shared::planet::kMarsScaleRatio1To200,
          "Scale applied to MOLA heights before building the preview mesh.");
ABSL_FLAG(double, obj_vertical_scale, 1.0, "Vertical scale multiplier applied to preview glTF heights.");

int main(int argc, char** argv)
{
    absl::SetProgramUsageMessage(
        "Inspect a Mars terrain TIFF or bake a local patch into grpcMMO-data.");
    absl::ParseCommandLine(argc, argv);

    try
    {
        const std::filesystem::path input_tiff = absl::GetFlag(FLAGS_input_tiff);
        if (absl::GetFlag(FLAGS_inspect_only))
        {
            const auto metadata = grpcmmo::tools::terrain::InspectRaster(input_tiff);
            std::cout << "Raster metadata" << std::endl;
            std::cout << "  input_tiff: " << std::filesystem::absolute(input_tiff).string()
                      << std::endl;
            std::cout << "  width: " << metadata.width << std::endl;
            std::cout << "  height: " << metadata.height << std::endl;
            std::cout << "  bits_per_sample: " << metadata.bits_per_sample << std::endl;
            std::cout << "  samples_per_pixel: " << metadata.samples_per_pixel << std::endl;
            std::cout << "  sample_format: "
                      << grpcmmo::tools::terrain::RasterSampleFormatName(metadata.sample_format)
                      << std::endl;
            return 0;
        }

        grpcmmo::tools::terrain::BakeSettings settings;
        settings.input_tiff = input_tiff;
        settings.output_dir = absl::GetFlag(FLAGS_output_dir);
        settings.planet_id = absl::GetFlag(FLAGS_planet_id);
        settings.patch_id = absl::GetFlag(FLAGS_patch_id);
        settings.center_lat_deg = absl::GetFlag(FLAGS_center_lat_deg);
        settings.center_lon_deg = absl::GetFlag(FLAGS_center_lon_deg);
        settings.lat_span_deg = absl::GetFlag(FLAGS_lat_span_deg);
        settings.lon_span_deg = absl::GetFlag(FLAGS_lon_span_deg);
        settings.output_rows = absl::GetFlag(FLAGS_output_rows);
        settings.output_cols = absl::GetFlag(FLAGS_output_cols);
        settings.planet_radius_m = absl::GetFlag(FLAGS_planet_radius_m);
        settings.height_scale = absl::GetFlag(FLAGS_height_scale);
        settings.obj_vertical_scale = absl::GetFlag(FLAGS_obj_vertical_scale);

        const auto result = grpcmmo::tools::terrain::BakeTerrainPatch(settings);
        std::cout << "Baked terrain patch" << std::endl;
        std::cout << "  patch_id: " << settings.patch_id << std::endl;
        std::cout << "  source_tiff: " << std::filesystem::absolute(settings.input_tiff).string()
                  << std::endl;
        std::cout << "  output_dir: " << std::filesystem::absolute(settings.output_dir).string()
                  << std::endl;
        std::cout << "  latitude_bounds_deg: [" << result.min_lat_deg << ", "
                  << result.max_lat_deg << "]" << std::endl;
        std::cout << "  longitude_bounds_deg: [" << result.min_lon_deg << ", "
                  << result.max_lon_deg << "]" << std::endl;
        std::cout << "  output_grid: " << settings.output_rows << " x "
                  << settings.output_cols << std::endl;
        std::cout << "  origin_height_m: " << result.origin_height_m << std::endl;
        std::cout << "  relative_height_range_m: [" << result.min_relative_height_m << ", "
                  << result.max_relative_height_m << "]" << std::endl;
        std::cout << "  height_file: " << result.height_file.string() << std::endl;
        std::cout << "  preview_gltf: " << result.preview_gltf_file.string() << std::endl;
        std::cout << "  metadata_file: " << result.metadata_file.string() << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "terrain tool failed: " << error.what() << std::endl;
        return 1;
    }
}
