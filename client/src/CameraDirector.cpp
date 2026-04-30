#include "CameraDirector.hpp"

namespace grpcmmo::client
{
void CameraDirector::Init()
{
    active_camera_ = nullptr;
}

void CameraDirector::End()
{
    for (auto& camera : cameras_)
    {
        camera->End();
    }
    cameras_.clear();
    cameras_by_id_.clear();
    active_camera_ = nullptr;
}

void CameraDirector::Tick(float delta_seconds)
{
    for (auto& camera : cameras_)
    {
        camera->Tick(delta_seconds);
    }
}

CameraActor& CameraDirector::CreateCamera(const std::string& camera_id)
{
    if (CameraActor* existing = FindCamera(camera_id); existing != nullptr)
    {
        return *existing;
    }

    auto camera = std::make_unique<CameraActor>();
    camera->SetCameraId(camera_id);
    camera->SetEntityId(camera_id);
    camera->SetDisplayName(camera_id);
    camera->Init();

    CameraActor* camera_ptr = camera.get();
    cameras_.push_back(std::move(camera));
    cameras_by_id_[camera_id] = camera_ptr;
    if (active_camera_ == nullptr)
    {
        active_camera_ = camera_ptr;
    }
    return *camera_ptr;
}

CameraActor* CameraDirector::FindCamera(const std::string& camera_id)
{
    const auto it = cameras_by_id_.find(camera_id);
    return it != cameras_by_id_.end() ? it->second : nullptr;
}

const CameraActor* CameraDirector::FindCamera(
    const std::string& camera_id) const
{
    const auto it = cameras_by_id_.find(camera_id);
    return it != cameras_by_id_.end() ? it->second : nullptr;
}

bool CameraDirector::SetActiveCamera(const std::string& camera_id)
{
    CameraActor* camera = FindCamera(camera_id);
    if (camera == nullptr)
    {
        return false;
    }

    active_camera_ = camera;
    return true;
}

CameraActor* CameraDirector::GetActiveCamera()
{
    return active_camera_;
}

const CameraActor* CameraDirector::GetActiveCamera() const
{
    return active_camera_;
}

bool CameraDirector::TryGetActiveCameraPose(CameraPose* pose) const
{
    if (pose == nullptr || active_camera_ == nullptr)
    {
        return false;
    }

    *pose = active_camera_->GetPose();
    return true;
}
} // namespace grpcmmo::client
