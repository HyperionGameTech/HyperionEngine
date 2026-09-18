/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Preview/AssetPreviewScene.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/PerspectiveCamera.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/Light.hpp>
#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/View.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/MeshComponent.hpp>

namespace Hyperion {

static constexpr float SubjectFramingPadding = 1.35f;

static const Vec3f ViewDirection = Vec3f(-0.55f, -0.45f, -0.7f).Normalized();

// Lights store the direction towards the light, so zero yaw/pitch places the light behind the
// viewer and lights the subject head-on.
static Vec3f ViewRelativeLightDirection(float yaw, float pitch)
{
    const Vec3f right = Vec3f::UnitY().Cross(ViewDirection).Normalized();
    const Vec3f up = ViewDirection.Cross(right).Normalized();

    const float cosPitch = MathUtil::Cos(pitch);

    return (right * (MathUtil::Sin(yaw) * cosPitch)
        + up * MathUtil::Sin(pitch)
        - ViewDirection * (MathUtil::Cos(yaw) * cosPitch))
        .Normalized();
}

AssetPreviewScene::AssetPreviewScene(Name name, Vec2u extent)
    : m_name(name),
      m_extent(MathUtil::Max(extent, Vec2u::One()))
{
}

AssetPreviewScene::~AssetPreviewScene()
{
    Shutdown();
}

bool AssetPreviewScene::Initialize(World* world)
{
    AssertOnThread(g_simThread);

    if (m_view.IsValid())
    {
        return true;
    }

    if (!world)
    {
        return false;
    }

    m_world = world;

    m_scene = MakeHandle<Scene>(NAME_FMT("{}Scene", m_name), SceneFlags::EDITOR | SceneFlags::HAS_OCTREE);
    InitObject(m_scene);

    m_world->AddScene(m_scene, /* addToStreamingLayer */ false);

    m_camera = MakeHandle<Camera>();
    m_camera->SetName(NAME_FMT("{}Camera", m_name));
    m_camera->AddCameraController(MakeHandle<PerspectiveCameraController>());
    m_camera->SetDimensions(Vec2i(int(m_extent.x), int(m_extent.y)));
    m_camera->SetFOV(40.0f);
    m_camera->SetNearClip(0.05f);
    m_camera->SetFarClip(1000.0f);
    InitObject(m_camera);
    m_scene->GetRoot()->AddChild(m_camera);

    m_keyLight = MakeHandle<DirectionalLight>(
        ViewRelativeLightDirection(MathUtil::DegToRad(-35.0f), MathUtil::DegToRad(35.0f)),
        Color(1.0f, 0.98f, 0.94f, 1.0f),
        4.0f);

    m_keyLight->SetName(NAME_FMT("{}KeyLight", m_name));
    m_keyLight->SetLightFlags(LightFlags::None);
    InitObject(m_keyLight);
    m_scene->GetRoot()->AddChild(m_keyLight);

    m_fillLight = MakeHandle<DirectionalLight>(
        Vec3f(0.5f, 0.2f, 0.8f).Normalized(),
        Color(0.6f, 0.7f, 0.85f, 1.0f),
        1.5f);

    m_fillLight->SetName(NAME_FMT("{}FillLight", m_name));
    m_fillLight->SetLightFlags(LightFlags::None);
    InitObject(m_fillLight);
    m_scene->GetRoot()->AddChild(m_fillLight);

    m_sphereMesh = MeshBuilder::NormalizedCubeSphere(6);
    m_sphereMesh->SetName(NAME_FMT("{}Sphere", m_name));
    m_sphereMesh->SetIsTransient(true);
    InitObject(m_sphereMesh);

    MaterialAttributes defaultAttributes;
    defaultAttributes.shaderName = NAME("GeometryPass");

    MaterialParameters defaultParameters;
    defaultParameters.albedo = Vec4f(0.8f, 0.8f, 0.82f, 1.0f);
    defaultParameters.roughness = 0.55f;
    defaultParameters.metalness = 0.0f;

    m_defaultMaterial = MakeHandle<Material>(
        NAME_FMT("{}Material", m_name),
        defaultAttributes,
        defaultParameters,
        MaterialTextures {});
    m_defaultMaterial->SetIsTransient(true);
    InitObject(m_defaultMaterial);

    m_entity = MakeHandle<Entity>();
    m_entity->SetName(NAME_FMT("{}Entity", m_name));
    m_scene->GetRoot()->AddChild(m_entity);

    MeshComponent meshComponent;
    meshComponent.mesh = m_sphereMesh;
    meshComponent.material = m_defaultMaterial;
    m_entity->AddComponent<MeshComponent>(meshComponent);
    m_entity->SetLocalBounds(m_sphereMesh->GetAABB());

    m_captureState = new ThumbnailCaptureState(m_extent);

    ViewDesc viewDesc {};
    viewDesc.flags = ViewFlags::GBUFFER
        | ViewFlags::THUMBNAIL_VIEW
        | ViewFlags::COLLECT_ALL_ENTITIES
        | ViewFlags::NOT_MULTI_BUFFERED
        | ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION
        | ViewFlags::NO_ASYNC_SHADER_LOADING
        | ViewFlags::NO_SHADOW_VIEWS
        | ViewFlags::SKIP_PARTICLE_VOLUMES
        | ViewFlags::SKIP_FOG_VOLUMES
        | ViewFlags::SKIP_SPRITES
        | ViewFlags::SKIP_CAMERAS
        | ViewFlags::SKIP_LIGHTMAP_VOLUMES;

    viewDesc.framebufferDesc.extent = m_extent;
    viewDesc.scenes = { m_scene.Get() };
    viewDesc.camera = m_camera.Get();

    m_view = MakeHandle<View>(viewDesc);
    m_view->SetName(NAME_FMT("{}View", m_name));
    m_view->thumbnailCaptureState = m_captureState;
    InitObject(m_view);

    FrameCameraToBounds(m_entity->GetLocalBounds());

    return true;
}

void AssetPreviewScene::Shutdown()
{
    if (m_view.IsValid())
    {
        m_view->thumbnailCaptureState = nullptr;

        EnqueueDeletion(std::move(m_view));
    }

    if (m_captureState)
    {
        // The render thread may still be inside a frame that referenced this; let it be torn down there
        // rather than pulling it out from under an in-flight capture.
        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            [captureState = m_captureState]()
            {
                delete captureState;
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        m_captureState = nullptr;
    }

    m_entity.Reset();
    m_keyLight.Reset();
    m_fillLight.Reset();
    m_camera.Reset();
    m_sphereMesh.Reset();
    m_defaultMaterial.Reset();

    if (m_scene.IsValid())
    {
        if (m_world)
        {
            m_world->RemoveScene(m_scene);
        }

        m_scene.Reset();
    }

    m_world = nullptr;
}

void AssetPreviewScene::ShowMaterial(Material* material)
{
    AssertOnThread(g_simThread);

    if (!m_entity.IsValid())
    {
        return;
    }

    MeshComponent* meshComponent = m_entity->TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return;
    }

    meshComponent->mesh = m_sphereMesh;
    meshComponent->material = material ? MakeStrongRef(material) : m_defaultMaterial;

    m_entity->AddTag<EntityTag::UpdateRenderProxy>();
    m_entity->SetLocalBounds(m_sphereMesh->GetAABB());

    FrameCameraToBounds(m_entity->GetLocalBounds());
}

void AssetPreviewScene::ShowMesh(Mesh* mesh)
{
    AssertOnThread(g_simThread);

    if (!m_entity.IsValid() || !mesh)
    {
        return;
    }

    MeshComponent* meshComponent = m_entity->TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return;
    }

    meshComponent->mesh = MakeStrongRef(mesh);
    meshComponent->material = m_defaultMaterial;

    meshComponent->mesh->UploadGpuData();

    m_entity->AddTag<EntityTag::UpdateRenderProxy>();
    m_entity->SetLocalBounds(meshComponent->mesh->GetAABB());

    FrameCameraToBounds(m_entity->GetLocalBounds());
}

void AssetPreviewScene::SetKeyLightDirection(const Vec3f& direction)
{
    AssertOnThread(g_simThread);

    if (!m_keyLight.IsValid())
    {
        return;
    }

    const float lengthSquared = direction.LengthSquared();

    if (lengthSquared <= MathUtil::epsilonF)
    {
        return;
    }

    m_keyLight->SetDirection(direction.Normalized());
}

void AssetPreviewScene::SetKeyLightViewAngles(float yaw, float pitch)
{
    AssertOnThread(g_simThread);

    SetKeyLightDirection(ViewRelativeLightDirection(yaw, pitch));
}

void AssetPreviewScene::Submit()
{
    AssertOnThread(g_simThread);

    if (m_world && m_view.IsValid())
    {
        m_world->ProcessViewAsync(m_view);
    }
}

void AssetPreviewScene::RequestCapture(ThumbnailCaptureState::Callback&& callback)
{
    if (m_captureState)
    {
        m_captureState->Request(std::move(callback));
    }
}

void AssetPreviewScene::FrameCameraToBounds(const BoundingBox& bounds)
{
    if (!m_camera.IsValid())
    {
        return;
    }

    const Vec3f center = bounds.IsValid() ? bounds.GetCenter() : Vec3f::Zero();

    float radius = bounds.IsValid() ? bounds.GetRadius() : 1.0f;
    radius = MathUtil::Clamp(radius, 0.01f, 500.0f);

    const float halfFov = MathUtil::DegToRad(m_camera->GetFOV() * 0.5f);
    const float distance = (radius * SubjectFramingPadding) / MathUtil::Max(MathUtil::Tan(halfFov), 0.0001f);

    m_camera->SetWorldTranslation(center - ViewDirection * distance);
    m_camera->SetDirection(ViewDirection);
    m_camera->SetFarClip(MathUtil::Max(distance + radius * 4.0f, 100.0f));
}

} // namespace Hyperion
