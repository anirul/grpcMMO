#include "TerrainPatchBaker.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "grpcmmo/shared/planet/PlanetMath.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <tiffio.h>

namespace grpcmmo::tools::terrain
{
namespace
{
constexpr std::string_view kHeightFileName = "ground_heights.f32";
constexpr std::string_view kPreviewGltfFileName = "ground_preview.gltf";
constexpr std::string_view kPreviewTextureFileName = "ground_preview_basecolor.png";
constexpr std::string_view kMetadataFileName = "patch.json";
constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr int kPreviewTextureSizePx = 512;
constexpr double kPreviewTextureRepeatCount = 96.0;

struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Vec2
{
    double x = 0.0;
    double y = 0.0;
};

struct RasterWindow
{
    RasterMetadata metadata{};
    std::uint32_t row_begin = 0;
    std::uint32_t row_end = 0;
    std::uint32_t col_begin = 0;
    std::uint32_t col_end = 0;
    std::vector<double> samples{};

    [[nodiscard]] std::uint32_t Width() const
    {
        return (col_end - col_begin) + 1;
    }

    [[nodiscard]] std::uint32_t Height() const
    {
        return (row_end - row_begin) + 1;
    }

    [[nodiscard]] double At(std::uint32_t row, std::uint32_t col) const
    {
        const std::size_t local_row = static_cast<std::size_t>(row - row_begin);
        const std::size_t local_col = static_cast<std::size_t>(col - col_begin);
        return samples[(local_row * Width()) + local_col];
    }

    [[nodiscard]] double SampleBilinear(double source_col, double source_row) const
    {
        source_col =
            std::clamp(source_col, static_cast<double>(col_begin), static_cast<double>(col_end));
        source_row =
            std::clamp(source_row, static_cast<double>(row_begin), static_cast<double>(row_end));

        const auto col0 = static_cast<std::uint32_t>(std::floor(source_col));
        const auto row0 = static_cast<std::uint32_t>(std::floor(source_row));
        const auto col1 = std::min(col0 + 1, col_end);
        const auto row1 = std::min(row0 + 1, row_end);

        const double tx = source_col - static_cast<double>(col0);
        const double ty = source_row - static_cast<double>(row0);

        const double top =
            std::lerp(At(row0, col0), At(row0, col1), tx);
        const double bottom =
            std::lerp(At(row1, col0), At(row1, col1), tx);
        return std::lerp(top, bottom, ty);
    }
};

struct TiffCloser
{
    void operator()(TIFF* handle) const
    {
        if (handle != nullptr)
        {
            TIFFClose(handle);
        }
    }
};

using UniqueTiffHandle = std::unique_ptr<TIFF, TiffCloser>;

[[nodiscard]] std::string EscapeJsonString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

[[nodiscard]] UniqueTiffHandle OpenRaster(const std::filesystem::path& input_tiff)
{
    if (!std::filesystem::exists(input_tiff))
    {
        throw std::runtime_error("input TIFF not found: " + input_tiff.string());
    }

    TIFF* handle = TIFFOpen(input_tiff.string().c_str(), "r");
    if (handle == nullptr)
    {
        throw std::runtime_error("failed to open TIFF: " + input_tiff.string());
    }
    return UniqueTiffHandle(handle);
}

[[nodiscard]] RasterSampleFormat ToRasterSampleFormat(std::uint16_t sample_format)
{
    switch (sample_format)
    {
    case SAMPLEFORMAT_UINT:
        return RasterSampleFormat::kUnsignedInteger;
    case SAMPLEFORMAT_INT:
        return RasterSampleFormat::kSignedInteger;
    case SAMPLEFORMAT_IEEEFP:
        return RasterSampleFormat::kFloat;
    default:
        return RasterSampleFormat::kUnknown;
    }
}

[[nodiscard]] RasterMetadata ReadRasterMetadata(TIFF* handle)
{
    RasterMetadata metadata;

    if (TIFFGetField(handle, TIFFTAG_IMAGEWIDTH, &metadata.width) != 1 ||
        TIFFGetField(handle, TIFFTAG_IMAGELENGTH, &metadata.height) != 1)
    {
        throw std::runtime_error("failed to read TIFF dimensions");
    }

    TIFFGetFieldDefaulted(handle, TIFFTAG_BITSPERSAMPLE, &metadata.bits_per_sample);
    TIFFGetFieldDefaulted(handle, TIFFTAG_SAMPLESPERPIXEL, &metadata.samples_per_pixel);

    std::uint16_t raw_sample_format = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(handle, TIFFTAG_SAMPLEFORMAT, &raw_sample_format);
    metadata.sample_format = ToRasterSampleFormat(raw_sample_format);

    if (metadata.samples_per_pixel != 1u)
    {
        throw std::runtime_error("only single-band TIFF rasters are supported");
    }

    const bool supported_bits =
        metadata.bits_per_sample == 8u || metadata.bits_per_sample == 16u ||
        metadata.bits_per_sample == 32u || metadata.bits_per_sample == 64u;
    if (!supported_bits)
    {
        throw std::runtime_error("unsupported TIFF sample size");
    }

    if (metadata.sample_format == RasterSampleFormat::kUnknown)
    {
        throw std::runtime_error("unsupported TIFF sample format");
    }

    return metadata;
}

[[nodiscard]] double DecodeSampleValue(const std::uint8_t* scanline,
                                       std::uint32_t column,
                                       const RasterMetadata& metadata)
{
    switch (metadata.sample_format)
    {
    case RasterSampleFormat::kUnsignedInteger:
        switch (metadata.bits_per_sample)
        {
        case 8u:
            return static_cast<double>(reinterpret_cast<const std::uint8_t*>(scanline)[column]);
        case 16u:
            return static_cast<double>(reinterpret_cast<const std::uint16_t*>(scanline)[column]);
        case 32u:
            return static_cast<double>(reinterpret_cast<const std::uint32_t*>(scanline)[column]);
        case 64u:
            return static_cast<double>(reinterpret_cast<const std::uint64_t*>(scanline)[column]);
        default:
            break;
        }
        break;
    case RasterSampleFormat::kSignedInteger:
        switch (metadata.bits_per_sample)
        {
        case 8u:
            return static_cast<double>(reinterpret_cast<const std::int8_t*>(scanline)[column]);
        case 16u:
            return static_cast<double>(reinterpret_cast<const std::int16_t*>(scanline)[column]);
        case 32u:
            return static_cast<double>(reinterpret_cast<const std::int32_t*>(scanline)[column]);
        case 64u:
            return static_cast<double>(reinterpret_cast<const std::int64_t*>(scanline)[column]);
        default:
            break;
        }
        break;
    case RasterSampleFormat::kFloat:
        switch (metadata.bits_per_sample)
        {
        case 32u:
            return static_cast<double>(reinterpret_cast<const float*>(scanline)[column]);
        case 64u:
            return reinterpret_cast<const double*>(scanline)[column];
        default:
            break;
        }
        break;
    case RasterSampleFormat::kUnknown:
        break;
    }

    throw std::runtime_error("unsupported TIFF sample layout");
}

[[nodiscard]] double SourceRowFromLatitude(const RasterMetadata& metadata, double latitude_deg)
{
    const double clamped_latitude = std::clamp(latitude_deg, -90.0, 90.0);
    return ((90.0 - clamped_latitude) / 180.0) *
           static_cast<double>(metadata.height - 1u);
}

[[nodiscard]] double SourceColFromLongitude(const RasterMetadata& metadata, double longitude_deg)
{
    const double clamped_longitude = std::clamp(longitude_deg, -180.0, 180.0);
    return ((clamped_longitude + 180.0) / 360.0) *
           static_cast<double>(metadata.width - 1u);
}

[[nodiscard]] double OutputLatitude(const BakeSettings& settings,
                                    std::uint32_t output_row,
                                    double min_lat_deg,
                                    double max_lat_deg)
{
    if (settings.output_rows <= 1u)
    {
        return settings.center_lat_deg;
    }

    const double t = static_cast<double>(output_row) /
                     static_cast<double>(settings.output_rows - 1u);
    return std::lerp(max_lat_deg, min_lat_deg, t);
}

[[nodiscard]] double OutputLongitude(const BakeSettings& settings,
                                     std::uint32_t output_col,
                                     double min_lon_deg,
                                     double max_lon_deg)
{
    if (settings.output_cols <= 1u)
    {
        return settings.center_lon_deg;
    }

    const double t = static_cast<double>(output_col) /
                     static_cast<double>(settings.output_cols - 1u);
    return std::lerp(min_lon_deg, max_lon_deg, t);
}

void ValidateBakeSettings(const BakeSettings& settings)
{
    if (settings.input_tiff.empty())
    {
        throw std::runtime_error("input TIFF path is required");
    }
    if (settings.output_dir.empty())
    {
        throw std::runtime_error("output directory is required");
    }
    if (settings.planet_id.empty())
    {
        throw std::runtime_error("planet_id is required");
    }
    if (settings.patch_id.empty())
    {
        throw std::runtime_error("patch_id is required");
    }
    if (settings.output_rows < 2u || settings.output_cols < 2u)
    {
        throw std::runtime_error("output_rows and output_cols must both be at least 2");
    }
    if (!(settings.lat_span_deg > 0.0 && settings.lat_span_deg <= 180.0))
    {
        throw std::runtime_error("lat_span_deg must be in the range (0, 180]");
    }
    if (!(settings.lon_span_deg > 0.0 && settings.lon_span_deg <= 360.0))
    {
        throw std::runtime_error("lon_span_deg must be in the range (0, 360]");
    }
    const double min_lat_deg = settings.center_lat_deg - (settings.lat_span_deg * 0.5);
    const double max_lat_deg = settings.center_lat_deg + (settings.lat_span_deg * 0.5);
    const double min_lon_deg = settings.center_lon_deg - (settings.lon_span_deg * 0.5);
    const double max_lon_deg = settings.center_lon_deg + (settings.lon_span_deg * 0.5);
    if (min_lat_deg < -90.0 || max_lat_deg > 90.0)
    {
        throw std::runtime_error("latitude bounds exceed [-90, 90]");
    }
    if (min_lon_deg < -180.0 || max_lon_deg > 180.0)
    {
        throw std::runtime_error("longitude bounds exceed [-180, 180]");
    }
    if (settings.planet_radius_m <= 0.0)
    {
        throw std::runtime_error("planet_radius_m must be positive");
    }
    if (settings.height_scale <= 0.0)
    {
        throw std::runtime_error("height_scale must be positive");
    }
    if (settings.obj_vertical_scale <= 0.0)
    {
        throw std::runtime_error("obj_vertical_scale must be positive");
    }
}

[[nodiscard]] RasterWindow LoadRasterWindow(TIFF* handle,
                                            const RasterMetadata& metadata,
                                            double min_lat_deg,
                                            double max_lat_deg,
                                            double min_lon_deg,
                                            double max_lon_deg)
{
    const double source_row_a = SourceRowFromLatitude(metadata, max_lat_deg);
    const double source_row_b = SourceRowFromLatitude(metadata, min_lat_deg);
    const double source_col_a = SourceColFromLongitude(metadata, min_lon_deg);
    const double source_col_b = SourceColFromLongitude(metadata, max_lon_deg);

    RasterWindow window;
    window.metadata = metadata;
    window.row_begin = static_cast<std::uint32_t>(std::max(
        0.0, std::floor(std::min(source_row_a, source_row_b)) - 1.0));
    window.row_end = static_cast<std::uint32_t>(std::min(
        static_cast<double>(metadata.height - 1u),
        std::ceil(std::max(source_row_a, source_row_b)) + 1.0));
    window.col_begin = static_cast<std::uint32_t>(std::max(
        0.0, std::floor(std::min(source_col_a, source_col_b)) - 1.0));
    window.col_end = static_cast<std::uint32_t>(std::min(
        static_cast<double>(metadata.width - 1u),
        std::ceil(std::max(source_col_a, source_col_b)) + 1.0));

    window.samples.resize(static_cast<std::size_t>(window.Width()) *
                          static_cast<std::size_t>(window.Height()));

    const auto scanline_size = static_cast<std::size_t>(TIFFScanlineSize(handle));
    std::vector<std::uint8_t> scanline(scanline_size);
    for (std::uint32_t row = window.row_begin; row <= window.row_end; ++row)
    {
        if (TIFFReadScanline(handle, scanline.data(), row, 0) != 1)
        {
            throw std::runtime_error("failed to read TIFF scanline");
        }

        for (std::uint32_t col = window.col_begin; col <= window.col_end; ++col)
        {
            const auto local_row = static_cast<std::size_t>(row - window.row_begin);
            const auto local_col = static_cast<std::size_t>(col - window.col_begin);
            window.samples[(local_row * window.Width()) + local_col] =
                DecodeSampleValue(scanline.data(), col, metadata);
        }
    }

    return window;
}

[[nodiscard]] Vec3 Subtract(const Vec3& lhs, const Vec3& rhs)
{
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vec3 Cross(const Vec3& lhs, const Vec3& rhs)
{
    return Vec3{(lhs.y * rhs.z) - (lhs.z * rhs.y),
                (lhs.z * rhs.x) - (lhs.x * rhs.z),
                (lhs.x * rhs.y) - (lhs.y * rhs.x)};
}

[[nodiscard]] double Length(const Vec3& value)
{
    return std::sqrt((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
}

[[nodiscard]] Vec3 Normalize(const Vec3& value)
{
    const double magnitude = Length(value);
    if (magnitude <= std::numeric_limits<double>::epsilon())
    {
        return Vec3{0.0, 1.0, 0.0};
    }
    return Vec3{value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] glm::dvec3 DirectionFromLatLonDegrees(double latitude_deg,
                                                    double longitude_deg)
{
    const double latitude_radians = latitude_deg * (std::numbers::pi_v<double> / 180.0);
    const double longitude_radians = longitude_deg * (std::numbers::pi_v<double> / 180.0);
    const double cos_latitude = std::cos(latitude_radians);
    return glm::dvec3(std::cos(longitude_radians) * cos_latitude,
                      std::sin(latitude_radians),
                      std::sin(longitude_radians) * cos_latitude);
}

[[nodiscard]] std::vector<Vec3> BuildVertexPositions(const BakeSettings& settings,
                                                     const std::vector<float>& relative_heights_m,
                                                     double origin_height_m,
                                                     double min_lat_deg,
                                                     double max_lat_deg,
                                                     double min_lon_deg,
                                                     double max_lon_deg)
{
    const double scaled_origin_height_m = origin_height_m * settings.height_scale;
    const glm::dvec3 center_direction =
        DirectionFromLatLonDegrees(settings.center_lat_deg, settings.center_lon_deg);
    const auto tangent_frame =
        grpcmmo::shared::planet::BuildTangentFrameFromUp(center_direction);
    const glm::dvec3 center_position =
        center_direction * (settings.planet_radius_m + scaled_origin_height_m);

    std::vector<Vec3> positions(relative_heights_m.size());

    for (std::uint32_t row = 0; row < settings.output_rows; ++row)
    {
        const double latitude_deg =
            OutputLatitude(settings, row, min_lat_deg, max_lat_deg);
        for (std::uint32_t col = 0; col < settings.output_cols; ++col)
        {
            const double longitude_deg =
                OutputLongitude(settings, col, min_lon_deg, max_lon_deg);
            const std::size_t index =
                static_cast<std::size_t>(row) * settings.output_cols + col;
            const glm::dvec3 direction =
                DirectionFromLatLonDegrees(latitude_deg, longitude_deg);
            const double absolute_height_m =
                scaled_origin_height_m +
                (static_cast<double>(relative_heights_m[index]) * settings.height_scale);
            const glm::dvec3 planet_position =
                direction * (settings.planet_radius_m + absolute_height_m);
            const glm::dvec3 local_offset = planet_position - center_position;

            positions[index] = Vec3{
                glm::dot(local_offset, tangent_frame.east),
                glm::dot(local_offset, tangent_frame.up) * settings.obj_vertical_scale,
                glm::dot(local_offset, tangent_frame.north)};
        }
    }

    return positions;
}

[[nodiscard]] std::vector<Vec3> BuildVertexNormals(const std::vector<Vec3>& positions,
                                                   std::uint32_t rows,
                                                   std::uint32_t cols)
{
    const auto index_of = [cols](std::uint32_t row, std::uint32_t col)
    {
        return static_cast<std::size_t>(row) * cols + col;
    };

    std::vector<Vec3> normals(positions.size(), Vec3{0.0, 1.0, 0.0});
    for (std::uint32_t row = 0; row < rows; ++row)
    {
        for (std::uint32_t col = 0; col < cols; ++col)
        {
            const auto left = positions[index_of(row, col == 0u ? 0u : col - 1u)];
            const auto right =
                positions[index_of(row, std::min(col + 1u, cols - 1u))];
            const auto north = positions[index_of(row == 0u ? 0u : row - 1u, col)];
            const auto south =
                positions[index_of(std::min(row + 1u, rows - 1u), col)];

            Vec3 normal = Normalize(Cross(Subtract(south, north), Subtract(right, left)));
            if (normal.y < 0.0)
            {
                normal.x = -normal.x;
                normal.y = -normal.y;
                normal.z = -normal.z;
            }

            normals[index_of(row, col)] = normal;
        }
    }
    return normals;
}

[[nodiscard]] std::vector<Vec2> BuildVertexTexcoords(std::uint32_t rows,
                                                     std::uint32_t cols)
{
    std::vector<Vec2> texcoords(static_cast<std::size_t>(rows) * cols);
    for (std::uint32_t row = 0; row < rows; ++row)
    {
        const double v = rows > 1u
            ? static_cast<double>(row) / static_cast<double>(rows - 1u)
            : 0.0;
        for (std::uint32_t col = 0; col < cols; ++col)
        {
            const double u = cols > 1u
                ? static_cast<double>(col) / static_cast<double>(cols - 1u)
                : 0.0;
            texcoords[static_cast<std::size_t>(row) * cols + col] = Vec2{
                u * kPreviewTextureRepeatCount,
                (1.0 - v) * kPreviewTextureRepeatCount};
        }
    }
    return texcoords;
}

[[nodiscard]] std::vector<std::uint32_t> BuildTriangleIndices(std::uint32_t rows,
                                                              std::uint32_t cols)
{
    const auto index_of = [cols](std::uint32_t row, std::uint32_t col)
    {
        return static_cast<std::uint32_t>(static_cast<std::size_t>(row) * cols + col);
    };

    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(rows - 1u) *
                    static_cast<std::size_t>(cols - 1u) * 6u);
    for (std::uint32_t row = 0; row + 1u < rows; ++row)
    {
        for (std::uint32_t col = 0; col + 1u < cols; ++col)
        {
            const auto i00 = index_of(row, col);
            const auto i10 = index_of(row, col + 1u);
            const auto i01 = index_of(row + 1u, col);
            const auto i11 = index_of(row + 1u, col + 1u);

            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i10);
            indices.push_back(i11);
            indices.push_back(i01);
        }
    }
    return indices;
}

void AppendAligned(std::vector<std::uint8_t>* buffer,
                   const void* data,
                   std::size_t byte_count)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    buffer->insert(buffer->end(), bytes, bytes + byte_count);
    while ((buffer->size() % 4u) != 0u)
    {
        buffer->push_back(0u);
    }
}

template <typename T>
void AppendArray(std::vector<std::uint8_t>* buffer, const std::vector<T>& values)
{
    if (values.empty())
    {
        return;
    }
    AppendAligned(buffer, values.data(), values.size() * sizeof(T));
}

[[nodiscard]] std::string Base64Encode(const std::vector<std::uint8_t>& bytes)
{
    std::string encoded;
    encoded.reserve(((bytes.size() + 2u) / 3u) * 4u);

    for (std::size_t index = 0; index < bytes.size(); index += 3u)
    {
        const std::uint32_t octet_a = bytes[index];
        const std::uint32_t octet_b = (index + 1u) < bytes.size() ? bytes[index + 1u] : 0u;
        const std::uint32_t octet_c = (index + 2u) < bytes.size() ? bytes[index + 2u] : 0u;
        const std::uint32_t triple = (octet_a << 16u) | (octet_b << 8u) | octet_c;

        encoded.push_back(kBase64Alphabet[(triple >> 18u) & 0x3Fu]);
        encoded.push_back(kBase64Alphabet[(triple >> 12u) & 0x3Fu]);
        encoded.push_back((index + 1u) < bytes.size()
                              ? kBase64Alphabet[(triple >> 6u) & 0x3Fu]
                              : '=');
        encoded.push_back((index + 2u) < bytes.size()
                              ? kBase64Alphabet[triple & 0x3Fu]
                              : '=');
    }

    return encoded;
}

void WritePreviewTexture(const std::filesystem::path& output_path)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kPreviewTextureSizePx * kPreviewTextureSizePx * 4),
        255u);

    constexpr int kMicroCellSize = 8;
    constexpr int kMinorCellSize = 32;
    constexpr int kMajorCellSize = 128;

    for (int y = 0; y < kPreviewTextureSizePx; ++y)
    {
        for (int x = 0; x < kPreviewTextureSizePx; ++x)
        {
            const bool micro_checker =
                (((x / kMicroCellSize) + (y / kMicroCellSize)) % 2) == 0;
            const bool minor_checker =
                (((x / kMinorCellSize) + (y / kMinorCellSize)) % 2) == 0;
            const bool micro_line =
                (x % kMicroCellSize == 0) || (y % kMicroCellSize == 0);
            const bool minor_line =
                (x % kMinorCellSize == 0) || (y % kMinorCellSize == 0);
            const bool major_u_line = (x % kMajorCellSize == 0);
            const bool major_v_line = (y % kMajorCellSize == 0);

            float red = micro_checker ? 118.0f : 102.0f;
            float green = minor_checker ? 86.0f : 72.0f;
            float blue = 54.0f;

            if (micro_line)
            {
                red *= 0.75f;
                green *= 0.75f;
                blue *= 0.75f;
            }
            if (minor_line)
            {
                red = 190.0f;
                green = 158.0f;
                blue = 110.0f;
            }
            if (major_u_line)
            {
                red = 232.0f;
                green = 92.0f;
                blue = 76.0f;
            }
            if (major_v_line)
            {
                red = 76.0f;
                green = 132.0f;
                blue = 232.0f;
            }

            const std::size_t offset =
                static_cast<std::size_t>((y * kPreviewTextureSizePx + x) * 4);
            pixels[offset + 0] =
                static_cast<std::uint8_t>(std::clamp(red, 0.0f, 255.0f));
            pixels[offset + 1] =
                static_cast<std::uint8_t>(std::clamp(green, 0.0f, 255.0f));
            pixels[offset + 2] =
                static_cast<std::uint8_t>(std::clamp(blue, 0.0f, 255.0f));
            pixels[offset + 3] = 255u;
        }
    }

    if (stbi_write_png(output_path.string().c_str(),
                       kPreviewTextureSizePx,
                       kPreviewTextureSizePx,
                       4,
                       pixels.data(),
                       kPreviewTextureSizePx * 4) == 0)
    {
        throw std::runtime_error("failed to write preview texture: " + output_path.string());
    }
}

void WriteHeightFile(const std::filesystem::path& output_path,
                     const std::vector<float>& relative_heights_m)
{
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        throw std::runtime_error("failed to write height file: " + output_path.string());
    }

    output.write(reinterpret_cast<const char*>(relative_heights_m.data()),
                 static_cast<std::streamsize>(relative_heights_m.size() * sizeof(float)));
}

void WritePreviewGltf(const std::filesystem::path& output_path,
                      const BakeSettings& settings,
                      const std::vector<float>& relative_heights_m,
                      double origin_height_m,
                      double min_lat_deg,
                      double max_lat_deg,
                      double min_lon_deg,
                      double max_lon_deg)
{
    const auto positions =
        BuildVertexPositions(settings, relative_heights_m, origin_height_m,
                             min_lat_deg, max_lat_deg,
                             min_lon_deg, max_lon_deg);
    const auto normals =
        BuildVertexNormals(positions, settings.output_rows, settings.output_cols);
    const auto texcoords =
        BuildVertexTexcoords(settings.output_rows, settings.output_cols);
    const auto indices =
        BuildTriangleIndices(settings.output_rows, settings.output_cols);

    std::ofstream output(output_path, std::ios::trunc);
    if (!output.is_open())
    {
        throw std::runtime_error("failed to write preview glTF: " + output_path.string());
    }

    std::vector<float> position_floats;
    position_floats.reserve(positions.size() * 3u);
    std::vector<float> normal_floats;
    normal_floats.reserve(normals.size() * 3u);
    std::vector<float> texcoord_floats;
    texcoord_floats.reserve(texcoords.size() * 2u);

    std::array<float, 3> min_position = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    std::array<float, 3> max_position = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    for (const auto& position : positions)
    {
        const float x = static_cast<float>(position.x);
        const float y = static_cast<float>(position.y);
        const float z = static_cast<float>(position.z);
        position_floats.push_back(x);
        position_floats.push_back(y);
        position_floats.push_back(z);
        min_position[0] = std::min(min_position[0], x);
        min_position[1] = std::min(min_position[1], y);
        min_position[2] = std::min(min_position[2], z);
        max_position[0] = std::max(max_position[0], x);
        max_position[1] = std::max(max_position[1], y);
        max_position[2] = std::max(max_position[2], z);
    }
    for (const auto& normal : normals)
    {
        normal_floats.push_back(static_cast<float>(normal.x));
        normal_floats.push_back(static_cast<float>(normal.y));
        normal_floats.push_back(static_cast<float>(normal.z));
    }
    for (const auto& texcoord : texcoords)
    {
        texcoord_floats.push_back(static_cast<float>(texcoord.x));
        texcoord_floats.push_back(static_cast<float>(texcoord.y));
    }

    std::vector<std::uint8_t> binary_blob;
    binary_blob.reserve((indices.size() * sizeof(std::uint32_t)) +
                        (position_floats.size() * sizeof(float)) +
                        (normal_floats.size() * sizeof(float)) +
                        (texcoord_floats.size() * sizeof(float)));

    const std::size_t indices_offset = binary_blob.size();
    AppendArray(&binary_blob, indices);
    const std::size_t positions_offset = binary_blob.size();
    AppendArray(&binary_blob, position_floats);
    const std::size_t normals_offset = binary_blob.size();
    AppendArray(&binary_blob, normal_floats);
    const std::size_t texcoords_offset = binary_blob.size();
    AppendArray(&binary_blob, texcoord_floats);

    const std::string data_uri =
        "data:application/octet-stream;base64," + Base64Encode(binary_blob);

    output << std::fixed << std::setprecision(6);
    output << "{\n";
    output << "  \"asset\": {\n";
    output << "    \"version\": \"2.0\"\n";
    output << "  },\n";
    output << "  \"scene\": 0,\n";
    output << "  \"scenes\": [\n";
    output << "    {\n";
    output << "      \"nodes\": [0]\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"nodes\": [\n";
    output << "    {\n";
    output << "      \"mesh\": 0\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"meshes\": [\n";
    output << "    {\n";
    output << "      \"primitives\": [\n";
    output << "        {\n";
    output << "          \"attributes\": {\n";
    output << "            \"POSITION\": 1,\n";
    output << "            \"NORMAL\": 2,\n";
    output << "            \"TEXCOORD_0\": 3\n";
    output << "          },\n";
    output << "          \"indices\": 0,\n";
    output << "          \"material\": 0\n";
    output << "        }\n";
    output << "      ]\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"materials\": [\n";
    output << "    {\n";
    output << "      \"name\": \"GroundPreview\",\n";
    output << "      \"pbrMetallicRoughness\": {\n";
    output << "        \"baseColorFactor\": [1.000000, 1.000000, 1.000000, 1.000000],\n";
    output << "        \"baseColorTexture\": {\n";
    output << "          \"index\": 0\n";
    output << "        },\n";
    output << "        \"metallicFactor\": 0.000000,\n";
    output << "        \"roughnessFactor\": 1.000000\n";
    output << "      }\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"samplers\": [\n";
    output << "    {\n";
    output << "      \"magFilter\": 9729,\n";
    output << "      \"minFilter\": 9987,\n";
    output << "      \"wrapS\": 10497,\n";
    output << "      \"wrapT\": 10497\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"images\": [\n";
    output << "    {\n";
    output << "      \"uri\": \"" << kPreviewTextureFileName << "\"\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"textures\": [\n";
    output << "    {\n";
    output << "      \"sampler\": 0,\n";
    output << "      \"source\": 0\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"buffers\": [\n";
    output << "    {\n";
    output << "      \"byteLength\": " << binary_blob.size() << ",\n";
    output << "      \"uri\": \"" << data_uri << "\"\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"bufferViews\": [\n";
    output << "    {\n";
    output << "      \"buffer\": 0,\n";
    output << "      \"byteOffset\": " << indices_offset << ",\n";
    output << "      \"byteLength\": " << (indices.size() * sizeof(std::uint32_t)) << ",\n";
    output << "      \"target\": 34963\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"buffer\": 0,\n";
    output << "      \"byteOffset\": " << positions_offset << ",\n";
    output << "      \"byteLength\": " << (position_floats.size() * sizeof(float)) << ",\n";
    output << "      \"target\": 34962\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"buffer\": 0,\n";
    output << "      \"byteOffset\": " << normals_offset << ",\n";
    output << "      \"byteLength\": " << (normal_floats.size() * sizeof(float)) << ",\n";
    output << "      \"target\": 34962\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"buffer\": 0,\n";
    output << "      \"byteOffset\": " << texcoords_offset << ",\n";
    output << "      \"byteLength\": " << (texcoord_floats.size() * sizeof(float)) << ",\n";
    output << "      \"target\": 34962\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"accessors\": [\n";
    output << "    {\n";
    output << "      \"bufferView\": 0,\n";
    output << "      \"componentType\": 5125,\n";
    output << "      \"count\": " << indices.size() << ",\n";
    output << "      \"type\": \"SCALAR\",\n";
    output << "      \"max\": [" << (positions.empty() ? 0u : static_cast<std::uint32_t>(positions.size() - 1u)) << "],\n";
    output << "      \"min\": [0]\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"bufferView\": 1,\n";
    output << "      \"componentType\": 5126,\n";
    output << "      \"count\": " << positions.size() << ",\n";
    output << "      \"type\": \"VEC3\",\n";
    output << "      \"max\": [" << max_position[0] << ", " << max_position[1] << ", " << max_position[2] << "],\n";
    output << "      \"min\": [" << min_position[0] << ", " << min_position[1] << ", " << min_position[2] << "]\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"bufferView\": 2,\n";
    output << "      \"componentType\": 5126,\n";
    output << "      \"count\": " << normals.size() << ",\n";
    output << "      \"type\": \"VEC3\"\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"bufferView\": 3,\n";
    output << "      \"componentType\": 5126,\n";
    output << "      \"count\": " << texcoords.size() << ",\n";
    output << "      \"type\": \"VEC2\",\n";
    output << "      \"max\": [" << kPreviewTextureRepeatCount << ", "
           << kPreviewTextureRepeatCount << "],\n";
    output << "      \"min\": [0.000000, 0.000000]\n";
    output << "    }\n";
    output << "  ]\n";
    output << "}\n";
}

void WriteMetadata(const std::filesystem::path& output_path,
                   const BakeSettings& settings,
                   const BakeResult& result)
{
    std::ofstream output(output_path, std::ios::trunc);
    if (!output.is_open())
    {
        throw std::runtime_error("failed to write patch metadata: " + output_path.string());
    }

    output << std::fixed << std::setprecision(6);
    output << "{\n";
    output << "  \"schema\": \"grpcmmo.terrain_patch.v1\",\n";
    output << "  \"planet_id\": \"" << EscapeJsonString(settings.planet_id) << "\",\n";
    output << "  \"patch_id\": \"" << EscapeJsonString(settings.patch_id) << "\",\n";
    output << "  \"source_tiff\": \""
           << EscapeJsonString(std::filesystem::absolute(settings.input_tiff).string())
           << "\",\n";
    output << "  \"output_dir\": \""
           << EscapeJsonString(std::filesystem::absolute(settings.output_dir).string())
           << "\",\n";
    output << "  \"center_lat_deg\": " << settings.center_lat_deg << ",\n";
    output << "  \"center_lon_deg\": " << settings.center_lon_deg << ",\n";
    output << "  \"min_lat_deg\": " << result.min_lat_deg << ",\n";
    output << "  \"max_lat_deg\": " << result.max_lat_deg << ",\n";
    output << "  \"min_lon_deg\": " << result.min_lon_deg << ",\n";
    output << "  \"max_lon_deg\": " << result.max_lon_deg << ",\n";
    output << "  \"planet_radius_m\": " << settings.planet_radius_m << ",\n";
    output << "  \"height_scale\": " << settings.height_scale << ",\n";
    output << "  \"origin_height_m\": " << result.origin_height_m << ",\n";
    output << "  \"source_raster\": {\n";
    output << "    \"width\": " << result.source_metadata.width << ",\n";
    output << "    \"height\": " << result.source_metadata.height << ",\n";
    output << "    \"bits_per_sample\": " << result.source_metadata.bits_per_sample << ",\n";
    output << "    \"samples_per_pixel\": " << result.source_metadata.samples_per_pixel << ",\n";
    output << "    \"sample_format\": \""
           << RasterSampleFormatName(result.source_metadata.sample_format) << "\"\n";
    output << "  },\n";
    output << "  \"height_grid\": {\n";
    output << "    \"rows\": " << settings.output_rows << ",\n";
    output << "    \"cols\": " << settings.output_cols << ",\n";
    output << "    \"encoding\": \"float32-le\",\n";
    output << "    \"file\": \"" << kHeightFileName << "\",\n";
    output << "    \"min_relative_height_m\": " << result.min_relative_height_m << ",\n";
    output << "    \"max_relative_height_m\": " << result.max_relative_height_m << "\n";
    output << "  },\n";
    output << "  \"preview_mesh\": {\n";
    output << "    \"file\": \"" << kPreviewGltfFileName << "\",\n";
    output << "    \"base_color_texture\": \"" << kPreviewTextureFileName << "\",\n";
    output << "    \"vertex_count\": " << result.vertex_count << ",\n";
    output << "    \"triangle_count\": " << result.triangle_count << ",\n";
    output << "    \"vertical_scale\": " << settings.obj_vertical_scale << "\n";
    output << "  }\n";
    output << "}\n";
}
} // namespace

RasterMetadata InspectRaster(const std::filesystem::path& input_tiff)
{
    auto handle = OpenRaster(input_tiff);
    return ReadRasterMetadata(handle.get());
}

BakeResult BakeTerrainPatch(const BakeSettings& settings)
{
    ValidateBakeSettings(settings);

    auto handle = OpenRaster(settings.input_tiff);
    const RasterMetadata metadata = ReadRasterMetadata(handle.get());

    const double min_lat_deg = settings.center_lat_deg - (settings.lat_span_deg * 0.5);
    const double max_lat_deg = settings.center_lat_deg + (settings.lat_span_deg * 0.5);
    const double min_lon_deg = settings.center_lon_deg - (settings.lon_span_deg * 0.5);
    const double max_lon_deg = settings.center_lon_deg + (settings.lon_span_deg * 0.5);

    const RasterWindow window =
        LoadRasterWindow(handle.get(), metadata, min_lat_deg, max_lat_deg, min_lon_deg, max_lon_deg);

    const double origin_height_m = window.SampleBilinear(
        SourceColFromLongitude(metadata, settings.center_lon_deg),
        SourceRowFromLatitude(metadata, settings.center_lat_deg));

    BakeResult result;
    result.source_metadata = metadata;
    result.min_lat_deg = min_lat_deg;
    result.max_lat_deg = max_lat_deg;
    result.min_lon_deg = min_lon_deg;
    result.max_lon_deg = max_lon_deg;
    result.origin_height_m = origin_height_m;
    result.relative_heights_m.resize(
        static_cast<std::size_t>(settings.output_rows) * settings.output_cols);

    result.min_relative_height_m = std::numeric_limits<float>::max();
    result.max_relative_height_m = std::numeric_limits<float>::lowest();

    for (std::uint32_t row = 0; row < settings.output_rows; ++row)
    {
        const double latitude_deg =
            OutputLatitude(settings, row, min_lat_deg, max_lat_deg);
        const double source_row = SourceRowFromLatitude(metadata, latitude_deg);
        for (std::uint32_t col = 0; col < settings.output_cols; ++col)
        {
            const double longitude_deg =
                OutputLongitude(settings, col, min_lon_deg, max_lon_deg);
            const double source_col = SourceColFromLongitude(metadata, longitude_deg);
            const double height_m = window.SampleBilinear(source_col, source_row);
            const float relative_height_m =
                static_cast<float>(height_m - origin_height_m);
            const auto index =
                static_cast<std::size_t>(row) * settings.output_cols + col;
            result.relative_heights_m[index] = relative_height_m;
            result.min_relative_height_m =
                std::min(result.min_relative_height_m, relative_height_m);
            result.max_relative_height_m =
                std::max(result.max_relative_height_m, relative_height_m);
        }
    }

    std::filesystem::create_directories(settings.output_dir);
    result.height_file =
        settings.output_dir / std::filesystem::path(std::string(kHeightFileName));
    result.preview_gltf_file =
        settings.output_dir / std::filesystem::path(std::string(kPreviewGltfFileName));
    result.preview_texture_file =
        settings.output_dir / std::filesystem::path(std::string(kPreviewTextureFileName));
    result.metadata_file =
        settings.output_dir / std::filesystem::path(std::string(kMetadataFileName));
    result.vertex_count = result.relative_heights_m.size();
    result.triangle_count =
        static_cast<std::size_t>(settings.output_rows - 1u) *
        static_cast<std::size_t>(settings.output_cols - 1u) * 2u;

    WriteHeightFile(result.height_file, result.relative_heights_m);
    WritePreviewTexture(result.preview_texture_file);
    WritePreviewGltf(result.preview_gltf_file,
                     settings,
                     result.relative_heights_m,
                     result.origin_height_m,
                     min_lat_deg,
                     max_lat_deg,
                     min_lon_deg,
                     max_lon_deg);
    WriteMetadata(result.metadata_file, settings, result);
    return result;
}

const char* RasterSampleFormatName(const RasterSampleFormat sample_format)
{
    switch (sample_format)
    {
    case RasterSampleFormat::kUnsignedInteger:
        return "uint";
    case RasterSampleFormat::kSignedInteger:
        return "int";
    case RasterSampleFormat::kFloat:
        return "float";
    case RasterSampleFormat::kUnknown:
        return "unknown";
    }
    return "unknown";
}
} // namespace grpcmmo::tools::terrain
