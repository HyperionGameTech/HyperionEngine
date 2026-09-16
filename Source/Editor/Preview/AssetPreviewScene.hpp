/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/ThumbnailCaptureState.hpp>

namespace Hyperion {

class World;
class Scene;
class Camera;
class Entity;
class DirectionalLight;
class View;
class Mesh;
class Material;

class AssetPreviewScene final
{
public:
    AssetPreviewScene(Name name, Vec2u extent);
    ~AssetPreviewScene();

    AssetPreviewScene(const AssetPreviewScene& other) = delete;
    AssetPreviewScene& operator=(const AssetPreviewScene& other) = delete;

    bool Initialize(World* world);
    void Shutdown();

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_view.IsValid();
    }

    HYP_FORCE_INLINE Vec2u GetExtent() const
    {
        return m_extent;
    }

    void ShowMaterial(Material* material);

    void ShowMesh(Mesh* mesh);

    void SetKeyLightDirection(const Vec3f& direction);

    /*! \brief Register the View for rendering this frame. ProcessViewAsync only lasts one frame, so this
     *  has to be called every sim tick for as long as the preview needs to render. */
    void Submit();

    /*! \brief Arm the capture; \p callback receives linear RGBA16F pixels on the render thread. */
    void RequestCapture(ThumbnailCaptureState::Callback&& callback);

    HYP_FORCE_INLINE bool IsCaptureRequested() const
    {
        return m_captureState != nullptr && m_captureState->IsRequested();
    }

private:
    void FrameCameraToBounds(const BoundingBox& bounds);

    Name m_name;
    Vec2u m_extent;

    World* m_world = nullptr;

    Handle<Scene> m_scene;
    Handle<Camera> m_camera;
    Handle<Entity> m_entity;
    Handle<DirectionalLight> m_keyLight;
    Handle<DirectionalLight> m_fillLight;
    Handle<View> m_view;

    Handle<Mesh> m_sphereMesh;
    Handle<Material> m_defaultMaterial;

    ThumbnailCaptureState* m_captureState = nullptr;
};

} // namespace Hyperion
