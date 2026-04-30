#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <tiffio.h>

#include "TerrainPatchBaker.hpp"

namespace grpcmmo::tools::terrain
{
namespace
{
class TemporaryDirectory
{
  public:
    TemporaryDirectory()
    {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path(
                    "grpcmmo-terrain-tests-" + std::to_string(stamp)
                );
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

void WriteFloatRaster(
    const std::filesystem::path& output_path,
    std::uint32_t width,
    std::uint32_t height,
    const std::vector<float>& samples
)
{
    ASSERT_EQ(samples.size(), static_cast<std::size_t>(width) * height);
    std::filesystem::create_directories(output_path.parent_path());

    TIFF* handle = TIFFOpen(output_path.string().c_str(), "w");
    ASSERT_NE(handle, nullptr);

    TIFFSetField(handle, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(handle, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(handle, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(handle, TIFFTAG_BITSPERSAMPLE, 32);
    TIFFSetField(handle, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
    TIFFSetField(handle, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(handle, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(handle, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

    std::vector<float> row(width);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            row[x] = samples[static_cast<std::size_t>(y) * width + x];
        }
        ASSERT_EQ(TIFFWriteScanline(handle, row.data(), y, 0), 1);
    }

    TIFFClose(handle);
}
} // namespace

TEST(TerrainPatchBakerTests, InspectRasterReadsMetadata)
{
    TemporaryDirectory temporary_directory;
    const auto raster_path = temporary_directory.Path() / "input.tif";
    const std::vector<float> samples = {0.0f,  1.0f,  2.0f,  3.0f,  4.0f,
                                        10.0f, 11.0f, 12.0f, 13.0f, 14.0f,
                                        20.0f, 21.0f, 22.0f, 23.0f, 24.0f,
                                        30.0f, 31.0f, 32.0f, 33.0f, 34.0f,
                                        40.0f, 41.0f, 42.0f, 43.0f, 44.0f};
    WriteFloatRaster(raster_path, 5u, 5u, samples);

    const auto metadata = InspectRaster(raster_path);
    EXPECT_EQ(metadata.width, 5u);
    EXPECT_EQ(metadata.height, 5u);
    EXPECT_EQ(metadata.bits_per_sample, 32u);
    EXPECT_EQ(metadata.samples_per_pixel, 1u);
    EXPECT_EQ(metadata.sample_format, RasterSampleFormat::kFloat);
}

TEST(TerrainPatchBakerTests, BakeTerrainPatchWritesExpectedOutputs)
{
    TemporaryDirectory temporary_directory;
    const auto raster_path = temporary_directory.Path() / "input.tif";
    const std::vector<float> samples = {0.0f,  1.0f,  2.0f,  3.0f,  4.0f,
                                        10.0f, 11.0f, 12.0f, 13.0f, 14.0f,
                                        20.0f, 21.0f, 22.0f, 23.0f, 24.0f,
                                        30.0f, 31.0f, 32.0f, 33.0f, 34.0f,
                                        40.0f, 41.0f, 42.0f, 43.0f, 44.0f};
    WriteFloatRaster(raster_path, 5u, 5u, samples);

    BakeSettings settings;
    settings.input_tiff = raster_path;
    settings.output_dir = temporary_directory.Path() / "patch";
    settings.planet_id = "mars";
    settings.patch_id = "patch-000";
    settings.center_lat_deg = 0.0;
    settings.center_lon_deg = 0.0;
    settings.lat_span_deg = 180.0;
    settings.lon_span_deg = 360.0;
    settings.output_rows = 5u;
    settings.output_cols = 5u;
    settings.planet_radius_m = 3396190.0;
    settings.height_scale = 1.0;
    settings.obj_vertical_scale = 1.0;

    const auto result = BakeTerrainPatch(settings);

    ASSERT_EQ(result.relative_heights_m.size(), 25u);
    EXPECT_FLOAT_EQ(result.relative_heights_m[0], -22.0f);
    EXPECT_FLOAT_EQ(result.relative_heights_m[12], 0.0f);
    EXPECT_FLOAT_EQ(result.relative_heights_m[24], 22.0f);
    EXPECT_FLOAT_EQ(result.min_relative_height_m, -22.0f);
    EXPECT_FLOAT_EQ(result.max_relative_height_m, 22.0f);
    EXPECT_EQ(result.vertex_count, 25u);
    EXPECT_EQ(result.triangle_count, 32u);

    EXPECT_TRUE(std::filesystem::exists(result.height_file));
    EXPECT_TRUE(std::filesystem::exists(result.preview_gltf_file));
    EXPECT_TRUE(std::filesystem::exists(result.preview_texture_file));
    EXPECT_TRUE(std::filesystem::exists(result.metadata_file));
    EXPECT_EQ(
        std::filesystem::file_size(result.height_file), 25u * sizeof(float)
    );

    std::ifstream metadata(result.metadata_file);
    ASSERT_TRUE(metadata.is_open());
    std::string metadata_contents(
        (std::istreambuf_iterator<char>(metadata)),
        std::istreambuf_iterator<char>()
    );
    EXPECT_NE(
        metadata_contents.find("\"patch_id\": \"patch-000\""), std::string::npos
    );
    EXPECT_NE(
        metadata_contents.find("\"origin_height_m\": 22.000000"),
        std::string::npos
    );
    EXPECT_NE(
        metadata_contents.find(
            "\"base_color_texture\": \"ground_preview_basecolor.png\""
        ),
        std::string::npos
    );
}
} // namespace grpcmmo::tools::terrain
