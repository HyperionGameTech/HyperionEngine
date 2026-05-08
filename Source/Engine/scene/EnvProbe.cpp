/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/EntityManager.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RendererMain.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderProxy.hpp>

#include <baking/BakerSubsystem.hpp>

#include <engine/EngineDriver.hpp>

#include <EnvProbe.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr EnumFlags<EnvProbeFlags> DefaultEnvProbeFlags[EPT_MAX] = {
    EPF_NONE,               // sky
    EPF_PARALLAX_CORRECTED, // reflection
    EPF_BAKED               // ambient
};

static const ShaderPropertyId s_propWriteNormals = InternShaderProperty(ShaderProperty(NAME("WRITE_NORMALS")));
static const ShaderPropertyId s_propWriteMoments = InternShaderProperty(ShaderProperty(NAME("WRITE_MOMENTS")));

static FixedArray<Mat4f, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Mat4f, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Mat4f::LookAt(
            origin,
            origin + Texture::s_cubemapDirections[i].first,
            Texture::s_cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region EnvProbe

EnvProbe::EnvProbe()
    : EnvProbe(EPT_INVALID)
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType)
    : EnvProbe(envProbeType, BoundingBox(Vec3f(-25.0f), Vec3f(25.0f)), Vec2u { 256, 256 })
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions)
    : m_dimensions(dimensions),
      m_envProbeType(envProbeType),
      m_envProbeFlags(DefaultEnvProbeFlags[envProbeType]),
      m_cameraNear(0.05f),
      m_cameraFar(aabb.GetRadius()),
      m_camera(nullptr),
      m_view(nullptr),
      m_shData {}
{
    SetLocalBounds(aabb);

    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = !(m_envProbeFlags & EPF_BAKED);
}

EnvProbe::~EnvProbe()
{
    if (m_view.IsValid())
    {
        EnqueueDeletion(std::move(m_view));
    }

    if (m_texture.IsValid())
    {
        EnqueueDeletion(std::move(m_texture));
    }

    if (m_camera != nullptr)
    {
        RemoveChild(m_camera);
        m_camera = nullptr;
    }
}

void EnvProbe::Init()
{
    Entity::Init();

    if (!IsBaked())
    {
        const BoundingBox worldBounds = GetWorldBounds();

        Handle<Camera> camera = MakeHandle<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_cameraNear, m_cameraFar);

        camera->SetName(NAME("EnvProbeCamera"));
        camera->SetViewMatrix(Mat4f::LookAt(worldBounds.GetCenter(), worldBounds.GetCenter() + Vec3f::UnitZ(), Vec3f::UnitY()));

        InitObject(camera);
        AddChild(camera);

        m_camera = camera;

        CreateView();

        if (ShouldComputePrefilteredEnvMap())
        {
            if (!m_texture)
            {
                m_texture = MakeHandle<Texture>(TextureDesc {
                    TextureType::Texture2D,
                    TextureFormat::RGBA16F,
                    Vec3u { m_dimensions, 1 },
                    TFM_LINEAR_MIPMAP,
                    TFM_LINEAR,
                    TWM_CLAMP_TO_EDGE,
                    1,
                    IU_STORAGE | IU_SAMPLED
                });

                m_texture->SetName(NAME_FMT("{}_{}_PrefilteredEnvMap", InstanceClass()->GetName(), GetName()));
            }
        }
    }

    if (m_texture.IsValid())
    {
        CheckResult(m_texture->Create());
    }

    SetReady(true);
}

void EnvProbe::SetEnvProbeFlags(EnumFlags<EnvProbeFlags> envProbeFlags)
{
    const uint32 changedFlags = envProbeFlags ^ m_envProbeFlags;

    if (!changedFlags)
    {
        return;
    }

    m_envProbeFlags = envProbeFlags;

    if (changedFlags & EPF_BAKED)
    {
        if (envProbeFlags[EPF_BAKED])
        {
            SetReceivesUpdate(false);
        }
        else
        {
            SetReceivesUpdate(true);
        }
    }

    MarkDirty();

    SetNeedsRenderProxyUpdate();
}

bool EnvProbe::IsVisible(ObjId<Camera> cameraId) const
{
    return m_visibilityBits.Test(cameraId.ToIndex());
}

void EnvProbe::SetIsVisible(ObjId<Camera> cameraId, bool isVisible)
{
    const bool previousValue = m_visibilityBits.Test(cameraId.ToIndex());

    m_visibilityBits.Set(cameraId.ToIndex(), isVisible);

    if (isVisible != previousValue)
    {
        Invalidate();
    }
}

void EnvProbe::OnAttachedToNode(Node* node)
{
    Entity::OnAttachedToNode(node);
}

void EnvProbe::OnDetachedFromNode(Node* node)
{
    Entity::OnDetachedFromNode(node);
}

void EnvProbe::OnAddedToWorld(World* world)
{
    Entity::OnAddedToWorld(world);
}

void EnvProbe::OnRemovedFromWorld(World* world)
{
    Entity::OnRemovedFromWorld(world);
}

void EnvProbe::OnAddedToScene(Scene* scene)
{
    Entity::OnAddedToScene(scene);

    if (m_view)
    {
        m_view->AddScene(scene);
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);

    if (m_view)
    {
        m_view->RemoveScene(scene);
    }

    Invalidate();
}

void EnvProbe::OnTransformUpdated()
{
    Entity::OnTransformUpdated();

    Invalidate();
}

void EnvProbe::CreateView()
{
    if (IsBaked())
    {
        return;
    }

    AssertDebug(m_view == nullptr);
    AssertDebug(m_camera != nullptr);

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = Vec2u(m_dimensions);
    framebufferDesc.numAttachments = 0;
    framebufferDesc.numLayers = 6;

    if (IsReflectionProbe() || IsSkyProbe())
    {
        framebufferDesc.AddAttachment(AttachmentDesc {
            TextureType::Cubemap,
            TextureFormat::R10G10B10A2,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        });

        framebufferDesc.AddAttachment(AttachmentDesc {
            TextureType::Cubemap,
            TextureFormat::RG16F,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        });

        AttachmentDesc momentsAttachmentDesc;
        momentsAttachmentDesc.imageType = TextureType::Cubemap;
        momentsAttachmentDesc.format = TextureFormat::RG16F;
        momentsAttachmentDesc.loadOp = LoadOperation::CLEAR;
        momentsAttachmentDesc.storeOp = StoreOperation::STORE;

        momentsAttachmentDesc.clearColorF16[0] = FLT16_MAX;
        momentsAttachmentDesc.clearColorF16[1] = FLT16_MAX;
        momentsAttachmentDesc.clearColorIsF16 = true;

        framebufferDesc.AddAttachment(momentsAttachmentDesc);
    }

    framebufferDesc.AddAttachment(AttachmentDesc {
        TextureType::Cubemap,
        TextureFormat::D32F,
        LoadOperation::CLEAR,
        StoreOperation::STORE
    });

    ShaderDesc shaderDesc;

    if (IsReflectionProbe())
    {
        shaderDesc.name = NAME("DrawCubemap");

        shaderDesc.properties.Add(s_propWriteNormals);
        shaderDesc.properties.Add(s_propWriteMoments);
    }
    else if (IsSkyProbe())
    {
        shaderDesc.name = NAME("RenderSky");
    }

    AssertDebug(shaderDesc.name.IsValid());

    ViewDesc viewDesc {
        .flags = (OnlyCollectStaticEntities() ? ViewFlags::COLLECT_STATIC_ENTITIES : ViewFlags::COLLECT_ALL_ENTITIES)
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION,
        .framebufferDesc = framebufferDesc,
        .scenes = {},
        .camera = m_camera,
        .overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shaderName = shaderDesc.name,
                .shaderProperties = shaderDesc.properties,
                .blendFunction = BlendFunction::AlphaBlending(),
                .cullFaces = FCM_NONE
            })
    };

    Handle<View> view = MakeHandle<View>(viewDesc);
    view->SetName(NAME_FMT("EnvProbe_{}_View", GetName()));
    InitObject(view);
    m_view = view;
}

void EnvProbe::SetOrigin(const Vec3f& origin)
{
    const Vec3f rel = origin - GetWorldTranslation();

    BoundingBox localBounds = GetLocalBounds();

    if (IsAmbientProbe())
    {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        const Vec3f extent = localBounds.GetExtent();

        localBounds.SetMin(rel);
        localBounds.SetMax(rel + extent);
    }
    else
    {
        localBounds.SetCenter(rel);
    }

    SetLocalBounds(localBounds);
}

void EnvProbe::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (IsBaked())
    {
        return;
    }

    const BoundingBox worldAabb = GetWorldBounds();

    bool needsUpdate = false;

    Array<ObjId<Scene>, SceneAllocator> cacheKeysToRemove;
    cacheKeysToRemove.Reserve(m_cachedOctantHashCodes.Size());

    for (const KeyValuePair<ObjId<Scene>, HashCode>& kvp : m_cachedOctantHashCodes)
    {
        cacheKeysToRemove.PushBack(kvp.first);
    }

    for (Scene* scene : m_view->GetScenes())
    {
        auto ApplyFrustumCheck = [&]()
        {
            // don't bother with the check for sky
            if (IsA(SkyProbe::StaticClass()))
            {
                needsUpdate = true;
                return;
            }

            for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ))
            {
                Camera* camera = static_cast<Camera*>(entity);

                if (camera->GetFrustum().ContainsAABB(worldAabb))
                {
                    needsUpdate = true;
                    return;
                }
            };
        };

        cacheKeysToRemove.Erase(scene->Id());

        if (scene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
        {
            HashCode octantHashCode = HashCode(0);

            const SceneOctree& octree = scene->GetOctree();

            SceneOctree const* octant = &octree;

            if (!octant)
            {
                continue;
            }

            if (OnlyCollectStaticEntities())
            {
                // clang-format off
                octantHashCode.Add(octant->GetOctantID().GetHashCode()
                    .Add(octant->GetEntryListHash<EntityTag::MobStatic>())
                    .Add(octant->GetEntryListHash<EntityTag::Light>()));
                // clang-format on
            }
            else
            {
                // clang-format off
                octantHashCode.Add(octree.GetOctantID().GetHashCode()
                    .Add(octree.GetEntryListHash<EntityTag::None>()));
                // clang-format on
            }

            auto it = m_cachedOctantHashCodes.Find(scene->Id());

            if (it == m_cachedOctantHashCodes.End())
            {
                if (!needsUpdate)
                    ApplyFrustumCheck();

                m_cachedOctantHashCodes[scene->Id()] = octantHashCode;

                continue;
            }

            if (it->second != octantHashCode)
            {
                if (!needsUpdate)
                    ApplyFrustumCheck();

                it->second = octantHashCode;

                continue;
            }
        }
        else
        {
            if (!needsUpdate)
                ApplyFrustumCheck();
        }
    }

    if (cacheKeysToRemove.Any())
    {
        for (ObjId<Scene> id : cacheKeysToRemove)
        {
            m_cachedOctantHashCodes.Erase(id);
        }
    }

    if (!needsUpdate)
        return;

    AssertDebug(m_camera != nullptr);
    m_camera->Update(delta);

    AssertDebug(m_view != nullptr);

    GetWorld()->ProcessViewAsync(m_view);
}

void EnvProbe::UpdateRenderProxy(RenderProxyEnvProbe* proxy)
{
    proxy->envProbe = WeakHandleFromThis();

    if (proxy->texture != m_texture)
    {
        // force texture to get rebound if we already have a texture but it has changed
        proxy->forceRebind = true;
    }

    proxy->texture = m_texture;

    const BoundingBox worldBounds = GetWorldBounds();

    EnvProbeShaderData& bufferData = proxy->bufferData;
    bufferData.aabbMin = Vec4f(worldBounds.min, 1.0f);
    bufferData.aabbMax = Vec4f(worldBounds.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.flags = uint32(m_envProbeFlags);

    const FixedArray<Mat4f, 6> viewMatrices = CreateCubemapMatrices(worldBounds, GetOrigin());

    Memory::Copy(bufferData.faceViewMatrices, viewMatrices.Data(), sizeof(EnvProbeShaderData::faceViewMatrices));
    Memory::Copy(bufferData.shData, &m_shData, sizeof(EnvProbeSphericalHarmonics::values));
}

void EnvProbe::SetBakedTexture(const Handle<Texture>& texture)
{
    if (m_texture == texture)
    {
        return;
    }

    if (m_texture != nullptr)
    {
        EnqueueDeletion(std::move(m_texture));
    }

    m_texture = texture;

    if (m_texture.IsValid())
    {
        CheckResult(m_texture->Create());
    }

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

#pragma endregion EnvProbe

#pragma region ReflectionProbe

#if HYP_EDITOR

void ReflectionProbe::BakeCubemap()
{
    HYP_SCOPE;

    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }

    BakerSubsystem* lightmapperSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!lightmapperSubsystem)
    {
        lightmapperSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    lightmapperSubsystem->EnqueueBake(MakeStrongRef(this));
}

#endif

#pragma endregion ReflectionProbe

#pragma region SkyProbe

void SkyProbe::Init()
{
    m_texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Cubemap,
        TextureFormat::RGBA16F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });

    m_texture->SetName(NAME_FMT("{}_SkyboxCubemap", Id()));
    m_texture->SetIsTransient(true);

    EnvProbe::Init();
}

#pragma endregion SkyProbe

} // namespace Hyperion
