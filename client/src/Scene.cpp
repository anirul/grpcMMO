#include "Scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "frame/json/proto.h"
#include "frame/json/serialize_uniform.h"
#include "frame/level_interface.h"
#include "frame/node_matrix.h"
#include "frame/window_interface.h"
#include "world/v1/replication.pb.h"

namespace grpcmmo::client
{
namespace
{
constexpr glm::vec3 kGroundScaleMeters(250.0f, 0.10f, 250.0f);
constexpr float kGroundTileHeight = -0.10f;
constexpr float kPawnLiftMeters = 0.70f;
constexpr glm::vec3 kPawnBodyScale(1.35f, 1.35f, 1.35f);

frame::proto::Texture MakeRenderTexture(const std::string& name)
{
    frame::proto::Texture texture;
    texture.set_name(name);
    texture.set_cubemap(false);
    texture.mutable_pixel_element_size()->set_value(frame::proto::PixelElementSize::BYTE);
    texture.mutable_pixel_structure()->set_value(frame::proto::PixelStructure::RGB);
    texture.mutable_size()->set_x(-1);
    texture.mutable_size()->set_y(-1);
    return texture;
}

frame::proto::Texture MakeSolidTexture(
    const std::string& name,
    const std::array<float, 4>& color,
    frame::proto::PixelElementSize::Enum element_size)
{
    frame::proto::Texture texture;
    texture.set_name(name);
    texture.set_cubemap(false);
    texture.mutable_size()->set_x(1);
    texture.mutable_size()->set_y(1);
    texture.mutable_pixel_structure()->set_value(frame::proto::PixelStructure::RGB_ALPHA);
    texture.mutable_pixel_element_size()->set_value(element_size);

    if (element_size == frame::proto::PixelElementSize::FLOAT)
    {
        texture.set_pixels(reinterpret_cast<const char*>(color.data()),
                           static_cast<int>(color.size() * sizeof(float)));
        return texture;
    }

    const std::array<std::uint8_t, 4> pixels = {
        static_cast<std::uint8_t>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(color[3], 0.0f, 1.0f) * 255.0f)};
    texture.set_pixels(reinterpret_cast<const char*>(pixels.data()),
                       static_cast<int>(pixels.size()));
    return texture;
}

frame::proto::Texture MakeByteTexture(
    const std::string& name,
    int width,
    int height,
    const std::vector<std::uint8_t>& pixels)
{
    frame::proto::Texture texture;
    texture.set_name(name);
    texture.set_cubemap(false);
    texture.mutable_size()->set_x(width);
    texture.mutable_size()->set_y(height);
    texture.mutable_pixel_structure()->set_value(frame::proto::PixelStructure::RGB_ALPHA);
    texture.mutable_pixel_element_size()->set_value(frame::proto::PixelElementSize::BYTE);
    texture.set_pixels(reinterpret_cast<const char*>(pixels.data()),
                       static_cast<int>(pixels.size()));
    return texture;
}

frame::proto::Texture MakeDebugCheckerTexture(const std::string& name)
{
    constexpr int kWidth = 512;
    constexpr int kHeight = 512;
    constexpr int kMicroCellSize = 8;
    constexpr int kMinorCellSize = 32;
    constexpr int kMajorCellSize = 128;
    constexpr int kArrowCellSize = 64;

    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kWidth * kHeight * 4),
        255);

    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            const float v = static_cast<float>(y) / static_cast<float>(kHeight - 1);

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
            const bool center_u = std::abs(x - (kWidth / 2)) <= 2;
            const bool center_v = std::abs(y - (kHeight / 2)) <= 2;

            const int arrow_u = x % kArrowCellSize;
            const int arrow_v = y % kArrowCellSize;
            const int arrow_center = kArrowCellSize / 2;
            const bool arrow_shaft =
                arrow_u >= 10 && arrow_u <= 36 &&
                std::abs(arrow_v - arrow_center) <= 1;
            const bool arrow_head =
                arrow_u >= 34 && arrow_u <= 52 &&
                std::abs(arrow_v - arrow_center) <=
                    ((arrow_u - 34) / 2 + 1);
            const bool arrow_marker = arrow_shaft || arrow_head;

            const bool u_tick =
                (x % kMajorCellSize >= 8 && x % kMajorCellSize <= 16) &&
                (y % kMajorCellSize >= 8 && y % kMajorCellSize <= 52);
            const bool v_tick =
                (y % kMajorCellSize >= 8 && y % kMajorCellSize <= 16) &&
                (x % kMajorCellSize >= 8 && x % kMajorCellSize <= 52);

            float red = 35.0f + (150.0f * u);
            float green = 45.0f + (150.0f * v);
            float blue = 80.0f + (55.0f * (1.0f - v));

            const float checker_scale = micro_checker ? 14.0f : -14.0f;
            red += checker_scale;
            green += checker_scale;
            blue += minor_checker ? 10.0f : -6.0f;

            if (micro_line)
            {
                red *= 0.55f;
                green *= 0.55f;
                blue *= 0.55f;
            }
            if (minor_line)
            {
                red = 220.0f;
                green = 220.0f;
                blue = 220.0f;
            }
            if (major_u_line)
            {
                red = 255.0f;
                green = 84.0f;
                blue = 84.0f;
            }
            if (major_v_line)
            {
                red = 84.0f;
                green = 156.0f;
                blue = 255.0f;
            }
            if (u_tick)
            {
                red = 255.0f;
                green = 196.0f;
                blue = 96.0f;
            }
            if (v_tick)
            {
                red = 96.0f;
                green = 240.0f;
                blue = 164.0f;
            }
            if (arrow_marker)
            {
                red = 255.0f;
                green = 232.0f;
                blue = 64.0f;
            }
            if (center_u)
            {
                red = 255.0f;
                green = 56.0f;
                blue = 56.0f;
            }
            if (center_v)
            {
                red = 56.0f;
                green = 156.0f;
                blue = 255.0f;
            }

            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset + 0] =
                static_cast<std::uint8_t>(std::clamp(red, 0.0f, 255.0f));
            pixels[offset + 1] =
                static_cast<std::uint8_t>(std::clamp(green, 0.0f, 255.0f));
            pixels[offset + 2] =
                static_cast<std::uint8_t>(std::clamp(blue, 0.0f, 255.0f));
            pixels[offset + 3] = 255;
        }
    }

    return MakeByteTexture(name, kWidth, kHeight, pixels);
}

frame::proto::Texture MakeCubemapTextureFromFile(
    const std::string& name,
    const std::string& file_name)
{
    frame::proto::Texture texture;
    texture.set_name(name);
    texture.set_cubemap(true);
    texture.mutable_pixel_structure()->set_value(frame::proto::PixelStructure::RGB);
    texture.mutable_pixel_element_size()->set_value(frame::proto::PixelElementSize::FLOAT);
    texture.set_file_name(file_name);
    return texture;
}

void AddDefaultRaytraceTextures(frame::proto::Level* level_proto)
{
    const auto add_byte_texture =
        [&](const std::string& name, const std::array<float, 4>& color)
    {
        *level_proto->add_textures() =
            MakeSolidTexture(name, color, frame::proto::PixelElementSize::BYTE);
    };
    const auto add_float_texture =
        [&](const std::string& name, const std::array<float, 4>& color)
    {
        *level_proto->add_textures() =
            MakeSolidTexture(name, color, frame::proto::PixelElementSize::FLOAT);
    };

    const std::array<float, 4> white = {1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> normal = {0.5f, 0.5f, 1.0f, 1.0f};
    const std::array<float, 4> black = {0.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> ior = {1.5f, 1.5f, 1.5f, 1.0f};
    const std::array<float, 4> far_attenuation = {1000000.0f, 1000000.0f, 1000000.0f, 1.0f};

    add_byte_texture("albedo_texture", white);
    add_byte_texture("Color", white);
    add_byte_texture("normal_texture", normal);
    add_byte_texture("roughness_texture", white);
    add_byte_texture("metallic_texture", black);
    add_byte_texture("ao_texture", white);
    add_byte_texture("specular_factor_texture", white);
    add_byte_texture("specular_color_texture", white);
    add_byte_texture("transmission_texture", black);
    add_float_texture("ior_texture", ior);
    add_float_texture("thickness_texture", black);
    add_byte_texture("attenuation_color_texture", white);
    add_float_texture("attenuation_distance_texture", far_attenuation);

    *level_proto->add_textures() = MakeDebugCheckerTexture("opaque_albedo_texture");
    add_byte_texture("opaque_normal_texture", normal);
    add_byte_texture("opaque_roughness_texture", white);
    add_byte_texture("opaque_metallic_texture", black);
    add_byte_texture("opaque_ao_texture", white);
    add_byte_texture("opaque_specular_factor_texture", white);
    add_byte_texture("opaque_specular_color_texture", white);

    add_byte_texture("transmissive_albedo_texture", white);
    add_byte_texture("transmissive_normal_texture", normal);
    add_byte_texture("transmissive_roughness_texture", white);
    add_byte_texture("transmissive_metallic_texture", black);
    add_byte_texture("transmissive_ao_texture", white);
    add_byte_texture("transmissive_transmission_texture", black);
    add_float_texture("transmissive_ior_texture", ior);
    add_float_texture("transmissive_thickness_texture", black);
    add_byte_texture("transmissive_attenuation_color_texture", white);
    add_float_texture("transmissive_attenuation_distance_texture", far_attenuation);
}

void SetIdentityMatrix(frame::proto::NodeMatrix* node)
{
    auto* matrix = node->mutable_matrix();
    matrix->set_m11(1.0f);
    matrix->set_m22(1.0f);
    matrix->set_m33(1.0f);
    matrix->set_m44(1.0f);
}

void AddIdentityMatrixNode(frame::proto::SceneTree* scene_tree,
                           const std::string& name,
                           const std::string& parent = std::string{})
{
    auto* node = scene_tree->add_node_matrices();
    node->set_name(name);
    node->set_matrix_type_enum(frame::proto::NodeMatrix::STATIC_MATRIX);
    if (!parent.empty())
    {
        node->set_parent(parent);
    }
    SetIdentityMatrix(node);
}

void AddStaticMatrixNode(frame::proto::SceneTree* scene_tree,
                         const std::string& name,
                         const glm::mat4& matrix,
                         const std::string& parent = std::string{})
{
    auto* node = scene_tree->add_node_matrices();
    node->set_name(name);
    node->set_matrix_type_enum(frame::proto::NodeMatrix::STATIC_MATRIX);
    if (!parent.empty())
    {
        node->set_parent(parent);
    }
    node->mutable_matrix()->CopyFrom(frame::json::SerializeUniformMatrix4(matrix));
}

void AddSceneEnumMeshNode(frame::proto::SceneTree* scene_tree,
                          const std::string& name,
                          const std::string& parent,
                          frame::proto::NodeMesh::MeshEnum mesh_enum,
                          frame::proto::NodeMesh::RenderTimeEnum render_time)
{
    auto* node = scene_tree->add_node_meshes();
    node->set_name(name);
    node->set_parent(parent);
    node->set_mesh_enum(mesh_enum);
    node->set_render_time_enum(render_time);
}

void AddSceneFileMeshNode(frame::proto::SceneTree* scene_tree,
                          const std::string& name,
                          const std::string& parent,
                          const std::string& file_name,
                          frame::proto::NodeMesh::RenderTimeEnum render_time)
{
    auto* node = scene_tree->add_node_meshes();
    node->set_name(name);
    node->set_parent(parent);
    node->set_file_name(file_name);
    node->set_render_time_enum(render_time);
}

glm::mat4 MakeTransform(glm::vec3 translation,
                        glm::quat rotation,
                        glm::vec3 scale)
{
    return glm::translate(glm::mat4(1.0f), translation) *
           glm::toMat4(glm::normalize(rotation)) *
           glm::scale(glm::mat4(1.0f), scale);
}

void SetNodeMatrix(frame::LevelInterface& level,
                   frame::EntityId node_id,
                   const glm::mat4& matrix)
{
    if (node_id == frame::NullId)
    {
        return;
    }

    auto& node = dynamic_cast<frame::NodeMatrix&>(level.GetSceneNodeFromId(node_id));
    auto& data = node.GetData();
    data.mutable_matrix()->CopyFrom(frame::json::SerializeUniformMatrix4(matrix));
    data.set_matrix_type_enum(frame::proto::NodeMatrix::STATIC_MATRIX);
    data.clear_quaternion();
}

glm::mat4 MakeHiddenTransform()
{
    return MakeTransform(glm::vec3(0.0f, -200.0f, 0.0f),
                         glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                         glm::vec3(0.01f));
}

} // namespace

void Scene::Init()
{
    window_ = nullptr;
    controlled_pawn_ = nullptr;
    camera_boon_ = nullptr;
    scene_handles_cached_ = false;
    ground_holder_matrix_id_ = frame::NullId;
    guide_holder_matrix_id_ = frame::NullId;
    landmark_holder_matrix_id_ = frame::NullId;
    pawn_root_matrix_id_ = frame::NullId;
    camera_boon_matrix_id_ = frame::NullId;
}

void Scene::End()
{
    Init();
}

void Scene::Tick(float /*delta_seconds*/)
{
    if (window_ == nullptr)
    {
        return;
    }

    auto& level = window_->GetDevice().GetLevel();
    if (!scene_handles_cached_)
    {
        CacheHandles(level);
        scene_handles_cached_ = true;
    }

    UpdateWorldHolders(level, controlled_pawn_);
    if (camera_boon_ != nullptr)
    {
        UpdatePawnRoot(level, controlled_pawn_, *camera_boon_);
    }
}

void Scene::SetDebugPoseTrace(bool enabled)
{
    debug_pose_trace_ = enabled;
}

void Scene::Attach(frame::WindowInterface* window)
{
    window_ = window;
}

void Scene::SetControlledState(const Pawn* controlled_pawn,
                               const CameraBoon* camera_boon)
{
    controlled_pawn_ = controlled_pawn;
    camera_boon_ = camera_boon;
}

CameraPose Scene::BuildCameraPose(const Pawn* controlled_pawn,
                                  const CameraBoon& camera_boon) const
{
    const float horizontal_distance =
        camera_boon.GetDistanceMeters() * std::cos(camera_boon.GetPitchRadians());
    const float vertical_distance =
        camera_boon.GetDistanceMeters() * std::sin(camera_boon.GetPitchRadians());

    if (controlled_pawn == nullptr)
    {
        CameraPose pose;
        const glm::vec3 focus(0.0f, camera_boon.GetFocusHeightMeters(), 0.0f);
        const glm::vec3 forward = BuildCameraForwardOnGround(camera_boon);
        pose.position =
            focus - (forward * horizontal_distance) + glm::vec3(0.0f, vertical_distance, 0.0f);
        pose.target = focus;
        pose.up = glm::vec3(0.0f, 1.0f, 0.0f);
        return pose;
    }

    const glm::vec3 up = controlled_pawn->GetSurfaceUp();
    const glm::vec3 focus =
        controlled_pawn->GetRenderPosition() + (up * camera_boon.GetFocusHeightMeters());
    const glm::vec3 forward = BuildCameraForwardOnGround(camera_boon);

    CameraPose pose;
    pose.position = focus - (forward * horizontal_distance) + (up * vertical_distance);
    pose.target = focus;
    pose.up = up;
    return pose;
}

void Scene::CacheHandles(frame::LevelInterface& level)
{
    ground_holder_matrix_id_ = level.GetIdFromName("ground_holder");
    guide_holder_matrix_id_ = level.GetIdFromName("guide_holder");
    landmark_holder_matrix_id_ = level.GetIdFromName("landmark_holder");
    pawn_root_matrix_id_ = level.GetIdFromName("pawn_root_matrix");
    camera_boon_matrix_id_ = level.GetIdFromName("camera_boon_matrix");
}

glm::vec3 Scene::BuildCameraForwardOnGround(const CameraBoon& camera_boon) const
{
    return glm::vec3(std::cos(camera_boon.GetYawRadians()),
                     0.0f,
                     std::sin(camera_boon.GetYawRadians()));
}

glm::vec3 Scene::BuildCameraBoonLocalOffset(const CameraBoon& camera_boon) const
{
    const float horizontal_distance =
        camera_boon.GetDistanceMeters() * std::cos(camera_boon.GetPitchRadians());
    const float vertical_distance =
        camera_boon.GetDistanceMeters() * std::sin(camera_boon.GetPitchRadians());
    return glm::vec3(-horizontal_distance,
                     camera_boon.GetFocusHeightMeters() + vertical_distance,
                     0.0f);
}

void Scene::UpdateWorldHolders(frame::LevelInterface& level,
                               const Pawn* controlled_pawn) const
{
    const glm::mat4 holder_transform =
        MakeTransform(glm::vec3(0.0f),
                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                      glm::vec3(1.0f));
    SetNodeMatrix(level, ground_holder_matrix_id_, holder_transform);
    SetNodeMatrix(level, guide_holder_matrix_id_, holder_transform);
    SetNodeMatrix(level, landmark_holder_matrix_id_, holder_transform);
}

void Scene::UpdatePawnRoot(frame::LevelInterface& level,
                           const Pawn* controlled_pawn,
                           const CameraBoon& camera_boon) const
{
    if (controlled_pawn == nullptr)
    {
        SetNodeMatrix(level, pawn_root_matrix_id_, MakeHiddenTransform());
        SetNodeMatrix(level, camera_boon_matrix_id_, MakeHiddenTransform());
        return;
    }

    const glm::quat pawn_orientation = controlled_pawn->GetRenderOrientation();
    SetNodeMatrix(level,
                  pawn_root_matrix_id_,
                  MakeTransform(controlled_pawn->GetRenderPosition(),
                                pawn_orientation,
                                glm::vec3(1.0f)));
    SetNodeMatrix(level,
                  camera_boon_matrix_id_,
                  MakeTransform(BuildCameraBoonLocalOffset(camera_boon),
                                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                glm::vec3(1.0f)));

    if (debug_pose_trace_)
    {
        const auto now = std::chrono::steady_clock::now();
        if ((now - last_pose_trace_at_) >= std::chrono::milliseconds(250))
        {
            last_pose_trace_at_ = now;
            const glm::vec3 pawn_position = controlled_pawn->GetRenderPosition();
            glm::vec3 node_position(0.0f);
            if (pawn_root_matrix_id_ != frame::NullId)
            {
                const auto& node = level.GetSceneNodeFromId(pawn_root_matrix_id_);
                const glm::mat4 node_model = node.GetLocalModel(0.0);
                node_position = glm::vec3(node_model[3]);
            }
            std::cout << "[pose] pawn=("
                      << pawn_position.x << "," << pawn_position.y << "," << pawn_position.z
                      << ") pawn_yaw=" << controlled_pawn->GetRenderYawRadians()
                      << " node=(" << node_position.x << "," << node_position.y << ","
                      << node_position.z << ")"
                      << " camera_yaw=" << camera_boon.GetYawRadians()
                      << std::endl;
        }
    }
}

frame::proto::Level Scene::BuildLevelProto() const
{
    frame::proto::Level level;
    level.set_name("grpcMMOThirdPerson");
    level.set_default_texture_name("albedo");
    *level.add_textures() = MakeRenderTexture("albedo");
    *level.add_textures() =
        MakeCubemapTextureFromFile("skybox", "asset/cubemap/shiodome.hdr");
    *level.add_textures() =
        MakeCubemapTextureFromFile("skybox_env", "asset/cubemap/shiodome_env.hdr");
    AddDefaultRaytraceTextures(&level);

    auto* scene_tree = level.mutable_scene_tree();
    scene_tree->set_default_root_name("root");
    scene_tree->set_default_camera_name("camera");

    AddIdentityMatrixNode(scene_tree, "root");
    AddIdentityMatrixNode(scene_tree, "env_holder", "root");
    AddIdentityMatrixNode(scene_tree, "mesh_holder", "root");
    AddIdentityMatrixNode(scene_tree, "ground_holder", "mesh_holder");
    AddIdentityMatrixNode(scene_tree, "guide_holder", "mesh_holder");
    AddIdentityMatrixNode(scene_tree, "landmark_holder", "mesh_holder");
    AddIdentityMatrixNode(scene_tree, "pawn_root_matrix", "mesh_holder");
    AddIdentityMatrixNode(scene_tree, "camera_boon_matrix", "pawn_root_matrix");
    AddStaticMatrixNode(scene_tree,
                        "ground_matrix",
                        MakeTransform(glm::vec3(0.0f, kGroundTileHeight, 0.0f),
                                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                      kGroundScaleMeters),
                        "ground_holder");
    AddStaticMatrixNode(scene_tree,
                        "pawn_body_matrix",
                        MakeTransform(glm::vec3(0.0f, kPawnLiftMeters, 0.0f),
                                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                      kPawnBodyScale),
                        "pawn_root_matrix");

    auto* camera = scene_tree->add_node_cameras();
    camera->set_name("camera");
    camera->set_parent("root");
    camera->set_fov_degrees(65.0f);
    camera->set_near_clip(0.05f);
    camera->set_far_clip(2000.0f);
    camera->mutable_position()->set_x(-18.0f);
    camera->mutable_position()->set_y(12.0f);
    camera->mutable_position()->set_z(-18.0f);
    camera->mutable_target()->set_x(0.0f);
    camera->mutable_target()->set_y(0.7f);
    camera->mutable_target()->set_z(0.0f);
    camera->mutable_up()->set_x(0.0f);
    camera->mutable_up()->set_y(1.0f);
    camera->mutable_up()->set_z(0.0f);

    AddSceneEnumMeshNode(scene_tree,
                         "CubeMapMesh",
                         "env_holder",
                         frame::proto::NodeMesh::CUBE,
                         frame::proto::NodeMesh::SKYBOX_RENDER_TIME);

    AddSceneFileMeshNode(scene_tree,
                         "GroundMesh",
                         "ground_matrix",
                         "cube.glb",
                         frame::proto::NodeMesh::SCENE_RENDER_TIME);

    AddSceneFileMeshNode(scene_tree,
                         "PawnBodyMesh",
                         "pawn_body_matrix",
                         "player_cube.gltf",
                         frame::proto::NodeMesh::SCENE_RENDER_TIME);

    auto* light = scene_tree->add_node_lights();
    light->set_name("sun");
    light->set_parent("root");
    light->set_light_type(frame::proto::NodeLight::DIRECTIONAL_LIGHT);
    light->set_shadow_type(frame::proto::NodeLight::HARD_SHADOW);
    light->mutable_direction()->set_x(0.7f);
    light->mutable_direction()->set_y(-1.0f);
    light->mutable_direction()->set_z(0.5f);
    light->mutable_color()->set_x(1.0f);
    light->mutable_color()->set_y(1.0f);
    light->mutable_color()->set_z(1.0f);

    return level;
}
} // namespace grpcmmo::client
