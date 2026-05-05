#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "PlanetMath.hpp"
#include "PreviewPatchConfig.hpp"
#include "grpcmmo/shared/WorkspaceConfig.hpp"

namespace grpcmmo::shared::planet
{
    class PreviewPatchTerrainSampler
    {
      public:
        [[nodiscard]] static double SampleTriangleSplitCellHeight(
            float north_west_height_m,
            float north_east_height_m,
            float south_west_height_m,
            float south_east_height_m,
            double row_t,
            double col_t
        )
        {
            const double clamped_row_t = Clamp01(row_t);
            const double clamped_col_t = Clamp01(col_t);

            // Match TerrainPatchBaker's triangle split:
            //   (north_west, north_east, south_west)
            //   (north_east, south_east, south_west)
            if ((clamped_row_t + clamped_col_t) <= 1.0)
            {
                return static_cast<double>(north_west_height_m) +
                       ((static_cast<double>(north_east_height_m) -
                         static_cast<double>(north_west_height_m)) *
                        clamped_col_t) +
                       ((static_cast<double>(south_west_height_m) -
                         static_cast<double>(north_west_height_m)) *
                        clamped_row_t);
            }

            const double north_east_weight = 1.0 - clamped_row_t;
            const double south_west_weight = 1.0 - clamped_col_t;
            const double south_east_weight =
                (clamped_row_t + clamped_col_t) - 1.0;
            return (static_cast<double>(north_east_height_m) * north_east_weight
                   ) +
                   (static_cast<double>(south_west_height_m) * south_west_weight
                   ) +
                   (static_cast<double>(south_east_height_m) * south_east_weight
                   );
        }

        [[nodiscard]] bool LoadPreviewPatch()
        {
            return Load();
        }

        [[nodiscard]] bool Load(
            const PreviewPatchConfig& config = kMarsPreviewPatch000
        )
        {
            Reset();
            config_ = config;
            const TangentFrame frame = BuildPreviewPatchFrame(config_);
            preview_patch_origin_planet_position_m_ =
                BuildPreviewPatchOriginPlanetPosition(config_);
            preview_patch_frame_east_ = frame.east;
            preview_patch_frame_up_ = frame.up;
            preview_patch_frame_north_ = frame.north;

            if (!grpcmmo::shared::kHaveDataRepo)
            {
                return false;
            }

            try
            {
                const std::filesystem::path patch_directory =
                    (std::filesystem::path(grpcmmo::shared::kDataRoot) /
                     "tiles" / config_.planet_id / config_.patch_id)
                        .lexically_normal();
                const std::filesystem::path metadata_path =
                    patch_directory / "patch.json";
                const std::filesystem::path height_path =
                    patch_directory / "ground_heights.f32";
                if (!std::filesystem::exists(metadata_path) ||
                    !std::filesystem::exists(height_path))
                {
                    return false;
                }

                const std::string metadata = ReadTextFile(metadata_path);
                min_lat_deg_ = ExtractJsonNumber(metadata, "min_lat_deg");
                max_lat_deg_ = ExtractJsonNumber(metadata, "max_lat_deg");
                min_lon_deg_ = ExtractJsonNumber(metadata, "min_lon_deg");
                max_lon_deg_ = ExtractJsonNumber(metadata, "max_lon_deg");
                source_planet_radius_m_ =
                    ExtractJsonNumber(metadata, "planet_radius_m");
                source_height_scale_ =
                    ExtractJsonNumber(metadata, "height_scale");
                source_origin_height_m_ =
                    ExtractJsonNumber(metadata, "origin_height_m");
                rows_ = static_cast<std::uint32_t>(
                    std::llround(ExtractJsonNumber(metadata, "rows"))
                );
                cols_ = static_cast<std::uint32_t>(
                    std::llround(ExtractJsonNumber(metadata, "cols"))
                );
                relative_heights_m_ =
                    ReadRelativeHeights(height_path, rows_, cols_);

                loaded_ =
                    rows_ > 0u && cols_ > 0u && !relative_heights_m_.empty();
                return loaded_;
            }
            catch (...)
            {
                relative_heights_m_.clear();
                min_lat_deg_ = 0.0;
                max_lat_deg_ = 0.0;
                min_lon_deg_ = 0.0;
                max_lon_deg_ = 0.0;
                source_planet_radius_m_ = 0.0;
                source_origin_height_m_ = 0.0;
                source_height_scale_ = 1.0;
                rows_ = 0u;
                cols_ = 0u;
                loaded_ = false;
                return false;
            }
        }

        void Reset()
        {
            relative_heights_m_.clear();
            config_ = kMarsPreviewPatch000;
            preview_patch_origin_planet_position_m_ = glm::dvec3(0.0, 0.0, 0.0);
            preview_patch_frame_east_ = glm::dvec3(1.0, 0.0, 0.0);
            preview_patch_frame_up_ = glm::dvec3(0.0, 1.0, 0.0);
            preview_patch_frame_north_ = glm::dvec3(0.0, 0.0, 1.0);
            min_lat_deg_ = 0.0;
            max_lat_deg_ = 0.0;
            min_lon_deg_ = 0.0;
            max_lon_deg_ = 0.0;
            source_planet_radius_m_ = 0.0;
            source_origin_height_m_ = 0.0;
            source_height_scale_ = 1.0;
            rows_ = 0u;
            cols_ = 0u;
            loaded_ = false;
        }

        [[nodiscard]] bool IsLoaded() const
        {
            return loaded_;
        }

        [[nodiscard]] const PreviewPatchConfig& Config() const
        {
            return config_;
        }

        [[nodiscard]] double SampleAbsoluteAltitudeM(
            const glm::dvec3& approx_world_position
        ) const
        {
            if (!loaded_)
            {
                return PreviewPatchSurfaceAltitudeM(config_);
            }

            const double relative_height_m =
                SampleRelativeHeightM(approx_world_position);
            return (source_origin_height_m_ * source_height_scale_) +
                   (relative_height_m * source_height_scale_);
        }

        [[nodiscard]] glm::dvec3 GroundWorldPosition(
            const glm::dvec3& approx_world_position
        ) const
        {
            return GroundWorldPosition(
                approx_world_position, config_.planet_radius_m
            );
        }

        [[nodiscard]] glm::dvec3 GroundWorldPosition(
            const glm::dvec3& approx_world_position, double planet_radius_m
        ) const
        {
            return ProjectToAltitude(
                approx_world_position,
                planet_radius_m,
                SampleAbsoluteAltitudeM(approx_world_position)
            );
        }

        [[nodiscard]] glm::vec3 GroundLocalPosition(
            const glm::vec3& local_position
        ) const
        {
            const glm::dvec3 local_position_d(
                local_position.x, local_position.y, local_position.z
            );
            const glm::dvec3 approx_world_position =
                preview_patch_origin_planet_position_m_ +
                LocalDirectionToWorld(
                    local_position_d,
                    TangentFrame{
                        preview_patch_frame_east_,
                        preview_patch_frame_north_,
                        preview_patch_frame_up_
                    }
                );

            const glm::dvec3 grounded_world_position =
                GroundWorldPosition(approx_world_position);
            const glm::dvec3 grounded_local_position = WorldPositionToLocal(
                grounded_world_position,
                preview_patch_origin_planet_position_m_,
                TangentFrame{
                    preview_patch_frame_east_,
                    preview_patch_frame_north_,
                    preview_patch_frame_up_
                }
            );
            return glm::vec3(
                static_cast<float>(grounded_local_position.x),
                static_cast<float>(grounded_local_position.y),
                static_cast<float>(grounded_local_position.z)
            );
        }

      private:
        [[nodiscard]] static std::string ReadTextFile(
            const std::filesystem::path& path
        )
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                throw std::runtime_error("failed to open " + path.string());
            }

            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        [[nodiscard]] static double ExtractJsonNumber(
            const std::string& text, const std::string& key
        )
        {
            const std::regex pattern(
                "\"" + key +
                "\"\\s*:\\s*([-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?)"
            );
            std::smatch match;
            if (!std::regex_search(text, match, pattern))
            {
                throw std::runtime_error(
                    "failed to find numeric key '" + key +
                    "' in terrain metadata"
                );
            }

            return std::stod(match[1].str());
        }

        [[nodiscard]] static std::vector<float> ReadRelativeHeights(
            const std::filesystem::path& path,
            std::uint32_t rows,
            std::uint32_t cols
        )
        {
            const std::size_t sample_count =
                static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
            std::vector<float> values(sample_count, 0.0f);

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                throw std::runtime_error("failed to open " + path.string());
            }

            input.read(
                reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(sample_count * sizeof(float))
            );
            if (!input || static_cast<std::size_t>(input.gcount()) !=
                              sample_count * sizeof(float))
            {
                throw std::runtime_error(
                    "failed to read expected terrain height sample count "
                    "from " +
                    path.string()
                );
            }

            return values;
        }

        [[nodiscard]] static double Clamp01(double value)
        {
            return std::clamp(value, 0.0, 1.0);
        }

        [[nodiscard]] double SampleRelativeHeightM(
            const glm::dvec3& approx_world_position_m
        ) const
        {
            const glm::dvec3 surface_up =
                SurfaceUpFromPosition(approx_world_position_m);
            const double latitude_deg =
                glm::degrees(std::asin(std::clamp(surface_up.y, -1.0, 1.0)));
            const double longitude_deg =
                glm::degrees(std::atan2(surface_up.z, surface_up.x));
            return SampleRelativeHeightM(latitude_deg, longitude_deg);
        }

        [[nodiscard]] double SampleRelativeHeightM(
            double latitude_deg, double longitude_deg
        ) const
        {
            if (!loaded_ || rows_ == 0u || cols_ == 0u)
            {
                return 0.0;
            }

            const double lat_range =
                std::max(max_lat_deg_ - min_lat_deg_, 0.000001);
            const double lon_range =
                std::max(max_lon_deg_ - min_lon_deg_, 0.000001);
            const double row =
                Clamp01((max_lat_deg_ - latitude_deg) / lat_range) *
                static_cast<double>(rows_ - 1u);
            const double col =
                Clamp01((longitude_deg - min_lon_deg_) / lon_range) *
                static_cast<double>(cols_ - 1u);

            const std::uint32_t row0 =
                static_cast<std::uint32_t>(std::floor(row));
            const std::uint32_t col0 =
                static_cast<std::uint32_t>(std::floor(col));
            const std::uint32_t row1 = std::min(row0 + 1u, rows_ - 1u);
            const std::uint32_t col1 = std::min(col0 + 1u, cols_ - 1u);
            const double row_t = row - static_cast<double>(row0);
            const double col_t = col - static_cast<double>(col0);

            const auto index_of =
                [this](std::uint32_t sample_row, std::uint32_t sample_col)
            {
                return (static_cast<std::size_t>(sample_row) * cols_) +
                       sample_col;
            };

            const float h00 = relative_heights_m_[index_of(row0, col0)];
            const float h10 = relative_heights_m_[index_of(row0, col1)];
            const float h01 = relative_heights_m_[index_of(row1, col0)];
            const float h11 = relative_heights_m_[index_of(row1, col1)];

            return SampleTriangleSplitCellHeight(
                h00, h10, h01, h11, row_t, col_t
            );
        }

        PreviewPatchConfig config_{};
        std::vector<float> relative_heights_m_{};
        glm::dvec3 preview_patch_origin_planet_position_m_{0.0, 0.0, 0.0};
        glm::dvec3 preview_patch_frame_east_{1.0, 0.0, 0.0};
        glm::dvec3 preview_patch_frame_up_{0.0, 1.0, 0.0};
        glm::dvec3 preview_patch_frame_north_{0.0, 0.0, 1.0};
        double min_lat_deg_ = 0.0;
        double max_lat_deg_ = 0.0;
        double min_lon_deg_ = 0.0;
        double max_lon_deg_ = 0.0;
        double source_planet_radius_m_ = 0.0;
        double source_origin_height_m_ = 0.0;
        double source_height_scale_ = 1.0;
        std::uint32_t rows_ = 0u;
        std::uint32_t cols_ = 0u;
        bool loaded_ = false;
    };
} // namespace grpcmmo::shared::planet
