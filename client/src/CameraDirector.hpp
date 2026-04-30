#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CameraActor.hpp"
#include "Object.hpp"

namespace grpcmmo::client
{
class CameraDirector : public Object
{
  public:
    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    CameraActor& CreateCamera(const std::string& camera_id);
    [[nodiscard]] CameraActor* FindCamera(const std::string& camera_id);
    [[nodiscard]] const CameraActor* FindCamera(const std::string& camera_id
    ) const;
    [[nodiscard]] bool SetActiveCamera(const std::string& camera_id);
    [[nodiscard]] CameraActor* GetActiveCamera();
    [[nodiscard]] const CameraActor* GetActiveCamera() const;
    [[nodiscard]] bool TryGetActiveCameraPose(CameraPose* pose) const;

  private:
    std::vector<std::unique_ptr<CameraActor>> cameras_;
    std::unordered_map<std::string, CameraActor*> cameras_by_id_;
    CameraActor* active_camera_ = nullptr;
};
} // namespace grpcmmo::client
