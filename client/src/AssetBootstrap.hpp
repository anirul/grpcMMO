#pragma once

#include <filesystem>

namespace frame
{
namespace proto
{
class Level;
}
} // namespace frame

namespace grpcmmo::client
{
class AssetBootstrap
{
  public:
    std::filesystem::path EnsureFrameAssetsAvailable() const;
    std::filesystem::path WriteGeneratedLevelJson(
        const frame::proto::Level& level) const;
};
} // namespace grpcmmo::client
