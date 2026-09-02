/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/View.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Light.hpp>
#include <Scene/EntityManager.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Asset/AssetRegistry.hpp>

#ifdef HYP_EDITOR
#include <Baking/BakerSubsystem.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#endif // HYP_EDITOR

#include <Framework/EngineDriver.hpp>
#include <Framework/GameState.hpp>

#include <EnvProbe.generated.inl>

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

static StaticShaderPropertyId s_propForwardShading { ShaderProperty(NAME("FORWARD_SHADING")) };
static StaticShaderPropertyId s_propApplyLightmaps { ShaderProperty(NAME("APPLY_LIGHTMAPS")) };
static StaticShaderPropertyId s_propWriteMoments { ShaderProperty(NAME("WRITE_MOMENTS")) };
static StaticShaderPropertyId s_propWriteHitMask { ShaderProperty(NAME("WRITE_HIT_MASK")) };

static constexpr EnumFlags<EnvProbeFlags> DefaultEnvProbeFlags[EPT_MAX] = {
    EPF_ORIGIN_FROM_CENTER,                                                                                 // sky
    EPF_ORIGIN_FROM_CENTER | EPF_BAKED | EPF_VISIBILITY | EPF_HIT_MASK | EPF_PARALLAX_CORRECTED,            // reflection
    EPF_ORIGIN_FROM_CENTER | EPF_BAKED | EPF_VISIBILITY | EPF_HIT_MASK                                      // irradiance
};

static constexpr EnvProbeDimensions DefaultDimensionsByType[EPT_MAX] = {
    SkyProbe::DefaultDimensions,
    ReflectionProbe::DefaultDimensions,
    IrradianceProbe::DefaultDimensions
};

static constexpr float EnvProbeCameraNearClip = 0.025f;

static FixedArray<Mat4f, 6> CreateCubemapMatrices(const Vec3f& origin)
{
    FixedArray<Mat4f, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Mat4f::LookAt(Texture::s_cubemapDirections[i].first, Texture::s_cubemapDirections[i].second)
            * Mat4f::Translation(-origin);
    }

    return viewMatrices;
}

#pragma region EnvProbe

EnvProbe::EnvProbe()
    : EnvProbe(EPT_INVALID)
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType)
    : EnvProbe(envProbeType, BoundingBox(Vec3f(-25.0f), Vec3f(25.0f)), DefaultDimensions)
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, EnvProbeDimensions dimensions)
    : m_dimensions(dimensions),
      m_envProbeType(envProbeType),
      m_envProbeFlags(DefaultEnvProbeFlags[envProbeType]),
      m_shData {},
      m_diffuseStrength(1.0f),
      m_camera(nullptr)
{
    SetLocalBounds(aabb);

    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = IsRealtime();
}

EnvProbe::~EnvProbe()
{
    // ensure locks are released before destruction ensues
    TUniqueResLock<EnvProbe> resLock(*this);

    if (AnyOf(m_views, &Handle<View>::IsValid))
    {
        EnqueueDeletion(std::move(m_views));
        EnqueueDeletion(std::move(m_framebuffers));
    }

    if (m_texture.IsValid())
    {
        EnqueueDeletion(std::move(m_texture));
    }
}

EnvProbeDimensions EnvProbe::GetDefaultDimensions(EnvProbeType envProbeType)
{
    return DefaultDimensionsByType[uint32(envProbeType)];
}

void EnvProbe::SetDimensions(EnvProbeDimensions dimensions)
{
    if (dimensions == m_dimensions)
    {
        return;
    }

    m_dimensions = dimensions;

    DestroyCaptureData();

    if (!(m_envProbeFlags & EPF_PATH_TRACED))
    {
        if (GetWorld() != nullptr)
        {
            InitCaptureData();
        }
    }

    // needs a re-render
    Invalidate(true);

    // needs editor to save it again
    MarkDirty();

    // and update the proxy, rebind the textures.
    SetNeedsRenderProxyUpdate();
}

void EnvProbe::SetDiffuseStrength(float diffuseStrength)
{
    diffuseStrength = MathUtil::Max(diffuseStrength, 0.0f);

    if (m_diffuseStrength == diffuseStrength)
    {
        return;
    }

    m_diffuseStrength = diffuseStrength;

    const bool shouldForceRerender = IsRealtime();

    if (shouldForceRerender)
    {
        Invalidate(shouldForceRerender);
    }

    MarkDirty();

    SetNeedsRenderProxyUpdate();
}

void EnvProbe::InitCaptureData()
{
    CreateCamera();
    CreateViewData();

    if (m_camera != nullptr)
    {
        const FixedArray<Mat4f, 6> matrices = CreateCubemapMatrices(GetWorldTranslation());

        for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
        {
            if (View* view = m_views[viewIndex])
            {
                view->cachedMatrices.view = matrices[viewIndex];
                view->cachedMatrices.viewProj = m_camera->GetProjectionMatrix() * matrices[viewIndex];
                view->cachedMatrices.invProj = m_camera->GetProjectionMatrix().Inverse();
            }
        }
    }

    EnqueueViewsUpdate();

    if (ShouldComputePrefilteredEnvMap())
    {
        if (!m_texture.IsValid())
        {
            Assert(uint32(m_dimensions) > 0);

            m_texture = MakeHandle<Texture>(TextureDesc {
                TextureType::Cubemap,
                TextureFormat::RGBA16F,
                Vec3u(Vec2u(uint32(m_dimensions)), 1),
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED
            });

            m_texture->SetName(NAME_FMT("{}_ColorMap", GetName()));

            if (IsSkyProbe())
            {
                // Sky's cubemap is realtime-rendered scratch data (matches SkyProbe::CreateTexture()),
                // not a persisted bake asset - don't register it below.
                m_texture->SetIsTransient(true);
            }
        }
    }

    if (m_texture.IsValid() && !IsSkyProbe())
    {
        GetCurrentAssetRegistry()->PutAssetUnique(m_texture);
    }

    if (m_envProbeFlags & EPF_VISIBILITY)
    {
        if (m_visibilityTexture.IsValid())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(m_visibilityTexture);
        }
        else
        {
            CreateVisibilityTexture();
        }
    }
}

void EnvProbe::DestroyCaptureData()
{
    RemoveCamera();
    DestroyViewData();

    if (IsRealtime() || !IsBaked())
    {
        EnqueueDeletion(std::move(m_texture));
        EnqueueDeletion(std::move(m_visibilityTexture));
    }
}

void EnvProbe::CreateCamera()
{
    if (m_camera != nullptr)
    {
        // Already created and set
        return;
    }

    const BoundingBox worldBounds = GetWorldBounds();

    // Try to find existing child of type Camera, if we are loading this EnvProbe
    auto cameraIt = GetChildren().FindIf(&ObjectBase::IsA<Camera>);
    if (cameraIt != GetChildren().End())
    {
        m_camera = StaticCast<Camera>(cameraIt->Get());

        InitObject(m_camera);

        m_camera->SetToPerspectiveProjection(90.0f, EnvProbeCameraNearClip, worldBounds.GetRadius());
    }
    
    if (!m_camera)
    {
        Handle<Camera> camera = MakeHandle<Camera>(
            90.0f,
            uint32(m_dimensions), uint32(m_dimensions),
            EnvProbeCameraNearClip, worldBounds.GetRadius());

        camera->SetName(NAME_FMT("{}_Capture", GetName()));
        AddChild(camera);

        m_camera = camera.Get();
    }

    m_camera->SetReceivesUpdate(false); // Don't automatically update
    m_camera->SetViewMatrix(Mat4f::LookAt(worldBounds.GetCenter(), worldBounds.GetCenter() + Vec3f::UnitZ(), Vec3f::UnitY()));

    InitObject(m_camera);
}

void EnvProbe::RemoveCamera()
{
    if (!m_camera)
    {
        return;
    }

    for (View* view : m_views)
    {
        if (view != nullptr)
        {
            view->SetCamera(nullptr);
        }
    }

    RemoveChild(m_camera, /* moveToDetached */ false);
    m_camera = nullptr;
}

void EnvProbe::CreateVisibilityTexture()
{
    if (m_visibilityTexture.IsValid())
    {
        GetCurrentAssetRegistry()->PutAssetUnique(m_visibilityTexture);

        return;
    }

    m_visibilityTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Cubemap,
        TextureFormat::RG16F,
        Vec3u {
            VisibilityTextureDimensions,
            VisibilityTextureDimensions,
            1
        },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE
    });

    m_visibilityTexture->SetName(NAME_FMT("{}_VisibilityMap", GetName()));

    GetCurrentAssetRegistry()->PutAssetUnique(m_visibilityTexture);

    // Assume the caller will MarkDirty() / SetNeedsRenderProxyUpdate()
}

void EnvProbe::SetEnvProbeFlags(EnumFlags<EnvProbeFlags> envProbeFlags)
{
    // If baked, can't be realtime - flags are mutually exclusive
    if (envProbeFlags & EPF_REALTIME)
    {
        envProbeFlags &= ~(EPF_BAKED | EPF_PATH_TRACED);
    }
    else
    {
        // Reflection and irradiance probes are baked through the Baker system
        // when not realtime.
        if (IsReflectionProbe() || IsAmbientProbe())
        {
            envProbeFlags |= EPF_BAKED;
        }
    }

    const uint32 changedFlags = envProbeFlags ^ m_envProbeFlags;

    if (!changedFlags)
    {
        return;
    }

    m_envProbeFlags = envProbeFlags;

    bool shouldForceRerender = false;
    bool dirtyViewData = false;

    // @TODO stupid overloads for EnumFlags... fix
    if ((changedFlags & uint32(EPF_BAKED | EPF_VISIBILITY | EPF_HIT_MASK | EPF_PATH_TRACED)) != 0)
    {
        dirtyViewData = true;
        shouldForceRerender = true;
    }

    if (changedFlags & EPF_REALTIME)
    {
        dirtyViewData = true;

        if (envProbeFlags & EPF_REALTIME)
        {
            SetReceivesUpdate(true);
        }
        else
        {
            SetReceivesUpdate(false);
        }
    }

    if (dirtyViewData)
    {
        // Baked (or path traced) probes have no persistent capture data
        if (envProbeFlags & (EPF_BAKED | EPF_PATH_TRACED))
        {
            DestroyCaptureData();
        }
        else
        {
            //--
            // ONLY INIT CAPTURE DATA IF ATTACHED TO A WORLD.
            // If not, defer it till OnAttachedToWorld().
            //--
            // If we don't do this, SetEnvProbeFlags() will be called before SetChildren(), meaning we'll create a camera then SetChildren() will overwrite the children,
            // we'll be left holding a dangling pointer for m_camera...
            //--
            if (GetWorld() != nullptr)
            {
                InitCaptureData();
            }
        }
    }

    if (shouldForceRerender)
    {
        Invalidate(/* forceRerender */ true);
    }

    MarkDirty();

    SetNeedsRenderProxyUpdate();
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

    // Init capture data right away for realtime probes, skybox, anything not BAKED.
    if (!IsBaked())
    {
        InitCaptureData();
        return;
    }

    if (m_envProbeFlags & EPF_VISIBILITY)
    {
        if (m_visibilityTexture.IsValid())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(m_visibilityTexture);
        }
        else
        {
            CreateVisibilityTexture();

            MarkDirty();
        }
    }
}

void EnvProbe::OnRemovedFromWorld(World* world)
{
    // Removing camera node before calling base method
    // @TODO: Do we need this?
    RemoveCamera();

    Entity::OnRemovedFromWorld(world);

    // @TODO: Ensure removing during bake won't cause major issues.
    DestroyCaptureData();
}

void EnvProbe::OnAddedToScene(Scene* scene)
{
    Entity::OnAddedToScene(scene);

    for (View* view : m_views)
    {
        if (view)
        {
            view->AddScene(scene);
        }
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);

    for (View* view : m_views)
    {
        if (view)
        {
            view->RemoveScene(scene);
        }
    }
}

void EnvProbe::OnTransformUpdated()
{
    Entity::OnTransformUpdated();

    Invalidate();
}

void EnvProbe::CreateViewData()
{
    if (EngineGlobals::IsHeadless())
    {
        // Do not create Views and other data when headless
        return;
    }
    // Not for path traced baked probes!
    Assert(!(m_envProbeFlags & EPF_PATH_TRACED));
    AssertDebug(m_camera != nullptr);

    // If any Views exist, we assume they have already been created
    for (View* view : m_views)
    {
        if (view != nullptr)
        {
            return;
        }
    }

    Array<AttachmentDesc> attachmentDescs;
    Array<GpuImageRef> attachmentImages;

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = Vec2u(uint32(m_dimensions));
    framebufferDesc.numAttachments = 0;
    framebufferDesc.numLayers = 6;

    FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
    Assert(framebuffer.IsValid());

    // Color target
    AttachmentDesc& colorDesc = attachmentDescs.PushBack(AttachmentDesc {
        TextureType::Cubemap,
        TextureFormat::RGBA16F,
        LoadOperation::CLEAR,
        StoreOperation::STORE
    });

    attachmentImages.PushBack(RI.MakeImage(TextureDesc {
        colorDesc.imageType,
        colorDesc.format,
        Vec3u(framebufferDesc.extent, 1),
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_ATTACHMENT }));

    // Visibility target
    // @FIXME: Needs to be created with HAS_VISIBILITY flag set for this to ever be created.
    // Setting HAS_VISIBILITY on an EnvProbe that wasn't created with it originally will cause us grievances.
    if (m_envProbeFlags & EPF_VISIBILITY)
    {
        AttachmentDesc& visibilityDesc = attachmentDescs.PushBack(AttachmentDesc {
            TextureType::Cubemap,
            TextureFormat::RG16F,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        });

        attachmentImages.PushBack(RI.MakeImage(TextureDesc {
            visibilityDesc.imageType,
            visibilityDesc.format,
            Vec3u(framebufferDesc.extent, 1),
            TFM_LINEAR,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_ATTACHMENT
        }));
    }

    if (m_envProbeFlags & EPF_HIT_MASK)
    {
        AttachmentDesc& hitMaskDesc = attachmentDescs.PushBack(AttachmentDesc {
            TextureType::Cubemap,
            TextureFormat::R8,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        });

        attachmentImages.PushBack(RI.MakeImage(TextureDesc {
            hitMaskDesc.imageType,
            hitMaskDesc.format,
            Vec3u(framebufferDesc.extent, 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_ATTACHMENT
        }));
    }

    // Depth target
    AttachmentDesc& depthDesc = attachmentDescs.PushBack(AttachmentDesc {
        TextureType::Cubemap,
        TextureFormat::D16,
        LoadOperation::CLEAR,
        StoreOperation::STORE
    });

    attachmentImages.PushBack(RI.MakeImage(TextureDesc {
        depthDesc.imageType,
        depthDesc.format,
        Vec3u(framebufferDesc.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_ATTACHMENT
    }));

    for (const GpuImageRef& image : attachmentImages)
    {
        Check(image->Create());
    }

    MaterialAttributes materialAttributes;

    if (IsSkyProbe())
    {
        materialAttributes.shaderName = NAME("RenderSky");
    }
    else
    {
        materialAttributes.shaderName = NAME("DrawCubemap");
        materialAttributes.shaderProperties.Add(s_propForwardShading);
        materialAttributes.shaderProperties.Add(s_propApplyLightmaps);

        if (m_envProbeFlags & EPF_VISIBILITY)
        {
            materialAttributes.shaderProperties.Add(s_propWriteMoments);
        }

        if (m_envProbeFlags & EPF_HIT_MASK)
        {
            materialAttributes.shaderProperties.Add(s_propWriteHitMask);
        }
    }

    AssertDebug(materialAttributes.shaderName.IsValid());

    for (uint16 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        FramebufferRef viewFramebuffer = RI.MakeFramebuffer(framebufferDesc);
        for (uint32 attachmentIndex = 0; attachmentIndex < uint32(attachmentDescs.Size()); attachmentIndex++)
        {
            const AttachmentDesc& attachmentDesc = attachmentDescs[attachmentIndex];
            const GpuImageRef& image = attachmentImages[attachmentIndex];

            // Create 2D view to the cubemap face
            GpuImageViewRef imageView = RI.MakeImageView(image, 0, 1, viewIndex, 1, TextureType::Texture2D);
            Check(imageView->Create());

            viewFramebuffer->AddAttachment(attachmentIndex, attachmentDesc, imageView);
        }

        Check(viewFramebuffer->Create());

        m_framebuffers[viewIndex] = std::move(viewFramebuffer);

        ViewDesc viewDesc {};
        viewDesc.flags = (OnlyCollectStaticEntities() ? ViewFlags::COLLECT_STATIC_ENTITIES : ViewFlags::COLLECT_ALL_ENTITIES)
            | ViewFlags::CUBEMAP_FACE_VIEW | ViewFlags::ENV_PROBE_VIEW
            | ViewFlags::NO_SHADOW_VIEWS
            | ViewFlags::NO_ASYNC_SHADER_LOADING
            | ViewFlags::EXTERNAL_RENDERTARGET;

        if (!IsRealtime())
        {
            viewDesc.flags |= (ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION | ViewFlags::NOT_MULTI_BUFFERED);
        }

        switch (m_envProbeType)
        {
        case EPT_SKY:
            viewDesc.flags |= ViewFlags::NO_SHADOW_VIEWS;
            // fallthrough
        case EPT_REFLECTION:
            break;
        case EPT_AMBIENT:
            viewDesc.flags |= ViewFlags::SKIP_ENV_PROBES;
            break;
        default:
            HYP_UNREACHABLE();
        }

        viewDesc.overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            materialAttributes);

        viewDesc.viewIndex = uint8(viewIndex);
        viewDesc.camera = m_camera;

        if (m_scene != nullptr)
        {
            viewDesc.scenes = { m_scene };
        }

        Handle<View> view = MakeHandle<View>(viewDesc);
        view->SetName(NAME_FMT("{}_{}_View{}", InstanceClass()->GetName(), GetName(), viewIndex));
        InitObject(view);

        m_views[viewIndex] = std::move(view);
    }
}

void EnvProbe::DestroyViewData()
{
    for (FramebufferRef& framebuffer : m_framebuffers)
    {
        if (framebuffer.IsValid())
        {
            EnqueueDeletion(std::move(framebuffer));
        }
    }

    for (Handle<View>& view : m_views)
    {
        if (view.IsValid())
        {
            view->RemoveScene(m_scene);

            EnqueueDeletion(std::move(view));
        }
    }
}

void EnvProbe::BeginRasterCapture()
{
    SetDimensions(GetDefaultDimensions(m_envProbeType));

    const int32 numReadbacks = (ShouldComputeSphericalHarmonics() ? 1 : 0)
        + ((m_envProbeFlags & EPF_VISIBILITY) ? 1 : 0)
        + ((m_envProbeFlags & EPF_HIT_MASK) ? 1 : 0);

    m_pendingCaptureReadbacks.Set(numReadbacks, MemoryOrder::RELEASE);

    InitCaptureData();

    needsRender.Store(true);
}

void EnvProbe::EndRasterCapture()
{
    DestroyCaptureData();
}

Vec3f EnvProbe::GetOrigin(bool fromCenter) const
{
    if (fromCenter)
    {
        return GetWorldBounds().GetCenter();
    }
    else
    {
        return GetWorldBounds().GetMin();
    }
}

void EnvProbe::SetOrigin(const Vec3f& origin, bool fromCenter)
{
    const Vec3f rel = origin - GetWorldTranslation();

    BoundingBox localBounds = GetLocalBounds();

    if (fromCenter)
    {
        localBounds.SetCenter(rel);
    }
    else
    {
        const Vec3f extent = localBounds.GetExtent();

        localBounds.SetMin(rel);
        localBounds.SetMax(rel + extent);
    }

    SetLocalBounds(localBounds);
}

void EnvProbe::SetSphericalHarmonicsData(const SphericalHarmonicsData& shData)
{
    if (m_shData == shData)
    {
        return;
    }

    m_shData = shData;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void EnvProbe::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const BoundingBox worldAabb = GetWorldBounds();

    bool needsUpdate = false;

    Array<ObjId<Scene>, SceneTempAllocator> cacheKeysToRemove;
    cacheKeysToRemove.Reserve(m_cachedOctantHashCodes.Size());

    for (const KeyValuePair<ObjId<Scene>, HashCode>& kvp : m_cachedOctantHashCodes)
    {
        cacheKeysToRemove.PushBack(kvp.first);
    }

    FixedArray<Mat4f, 6> matrices = CreateCubemapMatrices(GetWorldTranslation());

    Set<Scene*, SceneTempAllocator> allScenes;
    for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        // Update face view frustum.
        View* view = m_views[viewIndex];

        if (!view)
        {
            continue;
        }

        const Mat4f& viewMatrix = matrices[viewIndex];

        view->cachedMatrices.view = viewMatrix;
        view->cachedMatrices.viewProj = m_camera->GetProjectionMatrix() * viewMatrix;
        view->cachedMatrices.invProj = m_camera->GetProjectionMatrix().Inverse();

        for (Scene* scene : view->GetScenes())
        {
            allScenes.Add(scene);
        }
    }

    for (Scene* scene : allScenes)
    {
        // Check if the probe itself is in view.
        auto applyFrustumCheck = [&]()
        {
            // don't bother with the check for sky
            if (IsA(SkyProbe::StaticClass()))
            {
                needsUpdate = true;
                return;
            }

            for (auto [camera] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ))
            {
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
                {
                    applyFrustumCheck();
                }

                m_cachedOctantHashCodes[scene->Id()] = octantHashCode;

                continue;
            }

            if (it->second != octantHashCode)
            {
                if (!needsUpdate)
                {
                    applyFrustumCheck();
                }

                it->second = octantHashCode;

                continue;
            }
        }
        else
        {
            if (!needsUpdate)
            {
                applyFrustumCheck();
            }
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
    {
        return;
    }

    AssertDebug(m_camera != nullptr);

    if (m_camera != nullptr)
    {
        m_camera->Update(delta);
    }

    EnqueueViewsUpdate();
}

void EnvProbe::Invalidate(bool forceRerender)
{
    m_cachedOctantHashCodes.Clear();

    if (IsRealtime())
    {
        needsRender.Store(true);
    }
    else
    {
        if (forceRerender)
        {
            needsRender.Store(true);
        }

        if (!IsBaked())
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [weakThis = MakeWeakRef(this)]
                {
                    Handle<EnvProbe> strongThis = weakThis.Lock();
                    if (!strongThis.IsValid())
                    {
                        return;
                    }

                    World* world = strongThis->GetWorld();

                    if (world != nullptr)
                    {
                        strongThis->Update(world->GetGameState().deltaTime);
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void EnvProbe::EnqueueViewsUpdate()
{
    World* world = GetWorld();

    if (world != nullptr)
    {
        for (View* view : m_views)
        {
            world->ProcessViewAsync(view);
        }
    }
}

void EnvProbe::UpdateRenderProxy(RenderProxyEnvProbe* proxy)
{
    proxy->envProbe = this;

    if (proxy->texture != m_texture)
    {
        proxy->texture = m_texture.Get();
        proxy->forceRebind = true;
    }

    if (m_envProbeFlags & EPF_VISIBILITY)
    {
        if (proxy->visibilityTexture != m_visibilityTexture)
        {
            proxy->forceRebind = true;
            proxy->visibilityTexture = m_visibilityTexture.Get();
        }
    }
    else
    {
        if (proxy->visibilityTexture != nullptr)
        {
            proxy->forceRebind = true;
            proxy->visibilityTexture = nullptr;
        }
    }

    const BoundingBox worldBounds = GetWorldBounds();

    const float diffuseContributionWeight = ShouldComputeSphericalHarmonics() ? m_diffuseStrength : 0;

    EnvProbeShaderData& bufferData = proxy->bufferData;
    bufferData.aabbMin = Vec4f(worldBounds.min, m_camera ? m_camera->GetNearClip() : EnvProbeCameraNearClip);
    bufferData.aabbMax = Vec4f(worldBounds.max, m_camera ? m_camera->GetFarClip() : worldBounds.GetRadius());
    bufferData.worldPosition = Vec4f(GetWorldTranslation(), diffuseContributionWeight);
    bufferData.dimensions = Vec2u(uint32(m_dimensions));
    bufferData.typeAndFlags = uint32(m_envProbeType) | (uint32(m_envProbeFlags) << 3);

    // Update Spherical Harmonics data.
    const float* inSH = m_shData.values;
    Vec4f* outSH = bufferData.shData;

    for (size_t i = 0; i < 9; ++i)
    {
        outSH[i].x = *inSH++;
        outSH[i].y = *inSH++;
        outSH[i].z = *inSH++;
    }

    bufferData.hitMaskData = m_hitMaskData;
}

void EnvProbe::SetBakedTexture(const Handle<Texture>& texture)
{
    if (m_texture == texture)
    {
        return;
    }

    if (m_texture.IsValid())
    {
        EnqueueDeletion(std::move(m_texture));
    }

    m_texture = texture;

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

void EnvProbe::SetVisibilityTexture(const Handle<Texture>& visibilityTexture)
{
    if (m_visibilityTexture == visibilityTexture)
    {
        return;
    }

    if (m_visibilityTexture.IsValid())
    {
        EnqueueDeletion(std::move(m_visibilityTexture));
    }

    m_visibilityTexture = visibilityTexture;

    if (m_visibilityTexture.IsValid())
    {
        if (!(m_envProbeFlags & EPF_VISIBILITY))
        {
            HYP_LOG(Scene, Warning, "EnvProbe {} does not have visibility flag set, visibility texture will be unused unless the flag is set", GetName());
        }

        GetCurrentAssetRegistry()->PutAssetUnique(m_visibilityTexture);

        Invalidate(/* forceRerender */ true);
    }

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

void EnvProbe::SetHitMaskData(const Vec4f& hitMaskData)
{
    if (m_hitMaskData == hitMaskData)
    {
        return;
    }

    m_hitMaskData = hitMaskData;

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

#pragma endregion EnvProbe

#pragma region ReflectionProbe

#ifdef HYP_EDITOR

void ReflectionProbe::BakeCubemap()
{
    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }
    
    Handle<EditorSubsystem> editorSubsystem = g_editorState->GetEditorSubsystem();
    if (!editorSubsystem.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: No editor subsystem", GetName());

        return;
    }

    Handle<EditorProject> currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: No active project", GetName());

        return;
    }

    Baking::BakerScene& bakerScene = currentProject->GetBakerScene();

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(bakerScene, StaticCast<EnvProbe>(MakeStrongRef(this)));
}

#endif

#pragma endregion ReflectionProbe

#pragma region SkyProbe

void SkyProbe::CreateTexture()
{
    if (m_texture.IsValid())
    {
        return;
    }

    m_texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Cubemap,
        TextureFormat::RGBA16F,
        Vec3u(Vec2u(uint32(m_dimensions)), 1),
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });

    m_texture->SetName(NAME_FMT("{}_ColorMap", GetName()));
    m_texture->SetIsTransient(true);
}

#pragma endregion SkyProbe

#pragma region IrradianceProbe

#ifdef HYP_EDITOR

void IrradianceProbe::RecomputeIrradiance()
{
    if (!IsBaked())
    {
        Invalidate(true);

        return;
    }

    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }

    Handle<EditorSubsystem> editorSubsystem = g_editorState->GetEditorSubsystem();
    if (!editorSubsystem.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: No editor subsystem", GetName());

        return;
    }

    Handle<EditorProject> currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: No active project", GetName());

        return;
    }

    Baking::BakerScene& bakerScene = currentProject->GetBakerScene();

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(bakerScene, StaticCast<EnvProbe>(MakeStrongRef(this)));
}

#endif // HYP_EDITOR

void IrradianceProbe::Invalidate(bool forceRerender)
{
    EnvProbe::Invalidate(forceRerender);
}

#pragma endregion IrradianceProbe

} // namespace Hyperion
