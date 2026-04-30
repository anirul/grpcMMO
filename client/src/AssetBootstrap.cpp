#include "AssetBootstrap.hpp"

#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#include "WorkspacePaths.hpp"
#include "frame/json/proto.h"
#include "frame/json/serialize_json.h"
#include "grpcmmo/shared/WorkspaceConfig.hpp"

namespace grpcmmo::client
{
    namespace
    {
        void RemoveFileIfPresent(const std::filesystem::path& path)
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }

        void CopyFileIfChanged(
            const std::filesystem::path& source,
            const std::filesystem::path& destination
        )
        {
            std::ifstream in(source, std::ios::binary);
            if (!in.is_open())
            {
                throw std::runtime_error("failed to read " + source.string());
            }

            std::ostringstream source_buffer;
            source_buffer << in.rdbuf();
            const std::string source_data = source_buffer.str();

            bool should_write = true;
            std::ifstream existing(destination, std::ios::binary);
            if (existing.is_open())
            {
                std::ostringstream existing_buffer;
                existing_buffer << existing.rdbuf();
                should_write = existing_buffer.str() != source_data;
            }

            if (!should_write)
            {
                return;
            }

            std::filesystem::create_directories(destination.parent_path());
            std::ofstream out(destination, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                throw std::runtime_error(
                    "failed to write " + destination.string()
                );
            }
            out.write(
                source_data.data(),
                static_cast<std::streamsize>(source_data.size())
            );
        }

        std::string ReadTextFile(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                throw std::runtime_error("failed to read " + path.string());
            }

            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        void WriteTextFileIfChanged(
            const std::filesystem::path& destination,
            const std::string& contents
        )
        {
            bool should_write = true;
            std::ifstream existing(destination, std::ios::binary);
            if (existing.is_open())
            {
                std::ostringstream existing_buffer;
                existing_buffer << existing.rdbuf();
                should_write = existing_buffer.str() != contents;
            }

            if (!should_write)
            {
                return;
            }

            std::ofstream output(
                destination, std::ios::binary | std::ios::trunc
            );
            if (!output.is_open())
            {
                throw std::runtime_error(
                    "failed to write " + destination.string()
                );
            }
            output.write(
                contents.data(), static_cast<std::streamsize>(contents.size())
            );
        }

        std::size_t FindJsonSectionLineStart(
            std::string_view text, std::string_view key
        )
        {
            const std::size_t key_start = text.find(key);
            if (key_start == std::string::npos)
            {
                return std::string::npos;
            }

            const std::size_t line_start = text.rfind('\n', key_start);
            if (line_start == std::string::npos)
            {
                return 0;
            }

            return line_start + 1;
        }

        bool ReplaceJsonSection(
            std::string* text,
            std::string_view start_key,
            std::string_view next_key,
            const std::string& replacement
        )
        {
            const std::size_t start =
                FindJsonSectionLineStart(*text, start_key);
            if (start == std::string::npos)
            {
                return false;
            }

            const std::size_t next = FindJsonSectionLineStart(*text, next_key);
            if (next == std::string::npos || next <= start)
            {
                return false;
            }

            text->replace(start, next - start, replacement);
            return true;
        }

        void ApplyGroundPbrOverrideIfPresent(
            const std::filesystem::path& preview_gltf_path,
            const std::filesystem::path& destination_asset_root
        )
        {
            if (!std::filesystem::exists(preview_gltf_path))
            {
                return;
            }

            const std::filesystem::path material_directory =
                destination_asset_root / "model" / "ground_pbr";
            const std::filesystem::path diffuse_texture_path =
                material_directory / "red_laterite_soil_stones_diff_1k.jpg";
            const std::filesystem::path normal_texture_path =
                material_directory / "red_laterite_soil_stones_nor_gl_1k.jpg";
            const std::filesystem::path arm_texture_path =
                material_directory / "red_laterite_soil_stones_arm_1k.jpg";

            if (!std::filesystem::exists(diffuse_texture_path) ||
                !std::filesystem::exists(normal_texture_path) ||
                !std::filesystem::exists(arm_texture_path))
            {
                return;
            }

            std::string gltf = ReadTextFile(preview_gltf_path);
            const std::string material_section =
                "  \"materials\": [\n"
                "    {\n"
                "      \"name\": \"GroundPreview\",\n"
                "      \"pbrMetallicRoughness\": {\n"
                "        \"baseColorFactor\": [1.000000, 1.000000, 1.000000, "
                "1.000000],\n"
                "        \"baseColorTexture\": {\n"
                "          \"index\": 0\n"
                "        },\n"
                "        \"metallicFactor\": 1.000000,\n"
                "        \"roughnessFactor\": 1.000000,\n"
                "        \"metallicRoughnessTexture\": {\n"
                "          \"index\": 2\n"
                "        }\n"
                "      },\n"
                "      \"normalTexture\": {\n"
                "        \"index\": 1,\n"
                "        \"scale\": 1.000000\n"
                "      },\n"
                "      \"occlusionTexture\": {\n"
                "        \"index\": 2,\n"
                "        \"strength\": 1.000000\n"
                "      }\n"
                "    }\n"
                "  ],\n";
            const std::string images_section =
                "  \"images\": [\n"
                "    {\n"
                "      \"uri\": "
                "\"ground_pbr/red_laterite_soil_stones_diff_1k.jpg\"\n"
                "    },\n"
                "    {\n"
                "      \"uri\": "
                "\"ground_pbr/red_laterite_soil_stones_nor_gl_1k.jpg\"\n"
                "    },\n"
                "    {\n"
                "      \"uri\": "
                "\"ground_pbr/red_laterite_soil_stones_arm_1k.jpg\"\n"
                "    }\n"
                "  ],\n";
            const std::string textures_section = "  \"textures\": [\n"
                                                 "    {\n"
                                                 "      \"sampler\": 0,\n"
                                                 "      \"source\": 0\n"
                                                 "    },\n"
                                                 "    {\n"
                                                 "      \"sampler\": 0,\n"
                                                 "      \"source\": 1\n"
                                                 "    },\n"
                                                 "    {\n"
                                                 "      \"sampler\": 0,\n"
                                                 "      \"source\": 2\n"
                                                 "    }\n"
                                                 "  ],\n";

            const bool replaced_materials = ReplaceJsonSection(
                &gltf, "\"materials\": [", "\"samplers\": [", material_section
            );
            const bool replaced_images = ReplaceJsonSection(
                &gltf, "\"images\": [", "\"textures\": [", images_section
            );
            const bool replaced_textures = ReplaceJsonSection(
                &gltf, "\"textures\": [", "\"buffers\": [", textures_section
            );

            if (!replaced_materials || !replaced_images || !replaced_textures)
            {
                throw std::runtime_error(
                    "failed to apply ground material override to " +
                    preview_gltf_path.string()
                );
            }

            WriteTextFileIfChanged(preview_gltf_path, gltf);
        }
    } // namespace

    std::filesystem::path AssetBootstrap::EnsureFrameAssetsAvailable() const
    {
        const std::filesystem::path project_root = ResolveProjectRoot();
        const std::filesystem::path frame_root =
            NormalizePath(grpcmmo::shared::kFrameRoot);
        const std::filesystem::path source_asset_root = frame_root / "asset";
        const std::filesystem::path destination_asset_root =
            project_root / "asset";
        const std::filesystem::path preview_gltf_destination =
            destination_asset_root / "model" / "ground_preview.gltf";
        const std::filesystem::path preview_texture_destination =
            destination_asset_root / "model" / "ground_preview_basecolor.png";
        const std::filesystem::path preview_obj_destination =
            destination_asset_root / "model" / "ground_preview.obj";

        const std::array<std::filesystem::path, 3> source_directories = {
            source_asset_root / "shader",
            source_asset_root / "cubemap",
            source_asset_root / "model"
        };

        for (const auto& source_directory : source_directories)
        {
            if (!std::filesystem::exists(source_directory))
            {
                throw std::runtime_error(
                    "Frame asset directory not found: " +
                    source_directory.string()
                );
            }

            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(source_directory
                 ))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const auto relative =
                    std::filesystem::relative(entry.path(), source_asset_root);
                CopyFileIfChanged(
                    entry.path(), destination_asset_root / relative
                );
            }
        }

        if (grpcmmo::shared::kHaveDataRepo)
        {
            const std::filesystem::path terrain_preview_source = NormalizePath(
                std::filesystem::path(grpcmmo::shared::kDataRoot) / "tiles" /
                "mars" / "patch-000" / "ground_preview.gltf"
            );
            const std::filesystem::path terrain_preview_texture_source =
                NormalizePath(
                    std::filesystem::path(grpcmmo::shared::kDataRoot) /
                    "tiles" / "mars" / "patch-000" /
                    "ground_preview_basecolor.png"
                );
            if (std::filesystem::exists(terrain_preview_source))
            {
                CopyFileIfChanged(
                    terrain_preview_source, preview_gltf_destination
                );
                if (std::filesystem::exists(terrain_preview_texture_source))
                {
                    CopyFileIfChanged(
                        terrain_preview_texture_source,
                        preview_texture_destination
                    );
                }
                else
                {
                    RemoveFileIfPresent(preview_texture_destination);
                }
                RemoveFileIfPresent(preview_obj_destination);
                ApplyGroundPbrOverrideIfPresent(
                    preview_gltf_destination, destination_asset_root
                );
            }
            else
            {
                ApplyGroundPbrOverrideIfPresent(
                    preview_gltf_destination, destination_asset_root
                );
            }
        }
        else
        {
            ApplyGroundPbrOverrideIfPresent(
                preview_gltf_destination, destination_asset_root
            );
        }

        return destination_asset_root / "shader";
    }

    std::filesystem::path AssetBootstrap::WriteGeneratedLevelJson(
        const frame::proto::Level& level
    ) const
    {
        const std::filesystem::path project_root = ResolveProjectRoot();
        const std::filesystem::path output_path =
            project_root / "asset" / "generated" / "grpcmmo_third_person.json";
        std::filesystem::create_directories(output_path.parent_path());
        frame::json::SaveProtoToJsonFile(level, output_path);
        return output_path;
    }
} // namespace grpcmmo::client
