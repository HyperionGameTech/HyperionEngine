/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/Texture.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/RenderDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_API extern const GlobalConfig& GetGlobalConfig();

static const Vec2u shProbeDimensions { 256, 256 };

static const Vec2u lightFieldProbeDimensions { 32, 32 };
const TextureFormat lightFieldColorFormat = TF_RGBA8;
const TextureFormat lightFieldDepthFormat = TF_R16F;
static const uint32 irradianceOctahedronSize = 8;

static const Vec3u voxelGridDimensions { 256, 256, 256 };
static const TextureFormat voxelGridFormat = TF_RGBA8;

static const Vec2u framebufferDimensions { 256, 256 };

static Vec2u GetProbeDimensions(EnvGridType envGridType)
{
    switch (envGridType)
    {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return shProbeDimensions;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return lightFieldProbeDimensions;
    default:
        HYP_UNREACHABLE();
    }
}

#pragma region EnvProbeCollection

uint32 EnvProbeCollection::AddProbe(const Handle<EnvProbe>& envProbe)
{
    Assert(envProbe.IsValid());
    Assert(envProbe->IsReady());

    Assert(numProbes < g_maxBoundAmbientProbes);

    const uint32 index = numProbes++;

    envProbes[index] = envProbe;
    indirectIndices[index] = index;
    indirectIndices[g_maxBoundAmbientProbes + index] = index;

    return index;
}

// Must be called in EnvGrid::Init(), before probes are used from the render thread.
void EnvProbeCollection::AddProbe(uint32 index, const Handle<EnvProbe>& envProbe)
{
    Assert(envProbe.IsValid());
    Assert(envProbe->IsReady());

    Assert(index < g_maxBoundAmbientProbes);

    numProbes = MathUtil::Max(numProbes, index + 1);

    envProbes[index] = envProbe;
    indirectIndices[index] = index;
    indirectIndices[g_maxBoundAmbientProbes + index] = index;
}

#pragma region EnvProbeCollection

#pragma region EnvGrid

EnvGrid::EnvGrid()
    : EnvGrid(BoundingBox::Empty(), EnvGridOptions {})
{
}

EnvGrid::EnvGrid(const BoundingBox& aabb, const EnvGridOptions& options)
    : m_aabb(aabb),
      m_offset(aabb.GetCenter()),
      m_voxelGridAabb(aabb),
      m_options(options)
{
    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::Init()
{
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]
        {
            m_view.Reset();
            m_camera.Reset();
        }));

    const Vec2u probeDimensions = GetProbeDimensions(m_options.type);
    Assert(probeDimensions.Volume() != 0);

    CreateEnvProbes();

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(probeDimensions.x), int(probeDimensions.y),
        0.01f, m_aabb.GetRadius() * 2.0f);

    m_camera->SetName(Name::Unique("EnvGridCamera"));
    m_camera->SetTranslation(m_aabb.GetCenter());
    InitObject(m_camera);

    ShaderProperties shaderProperties(staticMeshVertexAttributes, { NAME("ENV_PROBE"), NAME("WRITE_NORMALS"), NAME("WRITE_MOMENTS") });

    ShaderDefinition shaderDefinition { NAME("RenderToCubemap"), shaderProperties };

    switch (GetEnvGridType())
    {
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
    {

        const Vec3u irradianceTextureDimensions = {
            (irradianceOctahedronSize + 2) * m_options.density.x * m_options.density.y + 2,
            (irradianceOctahedronSize + 2) * m_options.density.z + 2,
            1
        };

        ByteBuffer placeholderData;
        FillPlaceholderBuffer_Tex2D<lightFieldColorFormat>(irradianceTextureDimensions.GetXY(), placeholderData);

        m_irradianceTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                lightFieldColorFormat,
                irradianceTextureDimensions,
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });
        m_irradianceTexture->SetName(NAME_FMT("{}_LightFieldIrradiance", Id()));
        InitObject(m_irradianceTexture);

        m_depthTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                lightFieldDepthFormat,
                Vec3u {
                    (irradianceOctahedronSize + 2) * m_options.density.x * m_options.density.y + 2,
                    (irradianceOctahedronSize + 2) * m_options.density.z + 2,
                    1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });

        m_depthTexture->SetName(NAME_FMT("{}_LightFieldDepth", Id()));
        InitObject(m_depthTexture);

        break;
    }
    default:
        break;
    }

    if (m_options.flags & EnvGridFlags::USE_VOXEL_GRID)
    {

        // Create our voxel grid texture
        m_voxelGridTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX3D,
                voxelGridFormat,
                voxelGridDimensions,
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });

        m_voxelGridTexture->SetName(NAME_FMT("{}_VoxelGrid", Id()));
        InitObject(m_voxelGridTexture);
    }

    ViewOutputTargetDesc outputTargetDesc {
        .extent = Vec2u(framebufferDimensions),
        .attachments = {
            ViewOutputTargetAttachmentDesc {
                TF_RGBA8, // color
                TT_CUBEMAP,
                LoadOperation::CLEAR,
                StoreOperation::STORE },
            ViewOutputTargetAttachmentDesc {
                TF_RG16F, // normal
                TT_CUBEMAP,
                LoadOperation::CLEAR,
                StoreOperation::STORE },
            ViewOutputTargetAttachmentDesc {
                TF_R16, // distance, distance^2 (packed)
                TT_CUBEMAP,
                LoadOperation::CLEAR,
                StoreOperation::STORE },
            ViewOutputTargetAttachmentDesc {
                g_renderBackend->GetDefaultFormat(DIF_DEPTH),
                TT_CUBEMAP,
                LoadOperation::CLEAR,
                StoreOperation::STORE } },
        .numViews = 6
    };

    ViewDesc viewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::NOT_MULTI_BUFFERED,
        .viewport = Viewport { .extent = probeDimensions, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = {},
        .camera = m_camera,
        .overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes { .shaderDefinition = shaderDefinition, .cullFaces = FCM_BACK })
    };

    m_view = CreateObject<View>(viewDesc);

    InitObject(m_view);

    SetReady(true);
}

void EnvGrid::OnAttachedToNode(Node* node)
{
    HYP_SCOPE;
    Assert(IsReady());
    
    AttachChild(m_camera);

    for (const Handle<EnvProbe>& envProbe : m_envProbeCollection.envProbes)
    {
        AttachChild(envProbe);
    }

    // debugging
    Assert(node->FindChildWithEntity(m_envProbeCollection.envProbes[0]) != nullptr);
}

void EnvGrid::OnDetachedFromNode(Node* node)
{
    // detach EnvProbes
    HYP_SCOPE;
    
    DetachChild(m_camera);

    for (const Handle<EnvProbe>& envProbe : m_envProbeCollection.envProbes)
    {
        DetachChild(envProbe);
    }
}

void EnvGrid::OnAddedToWorld(World* world)
{
    Entity::OnAddedToWorld(world);
}

void EnvGrid::OnRemovedFromWorld(World* world)
{
    Entity::OnRemovedFromWorld(world);
}

void EnvGrid::OnAddedToScene(Scene* scene)
{
    Entity::OnAddedToScene(scene);

    m_view->AddScene(scene->HandleFromThis());
}

void EnvGrid::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);

    m_view->RemoveScene(scene->HandleFromThis());
}

void EnvGrid::CreateEnvProbes()
{
    HYP_SCOPE;

    const uint32 numAmbientProbes = m_options.density.Volume();
    const Vec3f aabbExtent = m_aabb.GetExtent();

    const Vec2u probeDimensions = GetProbeDimensions(m_options.type);
    Assert(probeDimensions.Volume() != 0);

    if (numAmbientProbes != 0)
    {
        for (uint32 x = 0; x < m_options.density.x; x++)
        {
            for (uint32 y = 0; y < m_options.density.y; y++)
            {
                for (uint32 z = 0; z < m_options.density.z; z++)
                {
                    const uint32 index = x * m_options.density.x * m_options.density.y
                        + y * m_options.density.x
                        + z;

                    const BoundingBox envProbeAabb(
                        m_aabb.min + (Vec3f(float(x), float(y), float(z)) * SizeOfProbe()),
                        m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * SizeOfProbe()));

                    Handle<EnvProbe> envProbe = CreateObject<EnvProbe>(EPT_AMBIENT, envProbeAabb, probeDimensions);
                    envProbe->m_gridSlot = index;
                    envProbe->m_positionInGrid = Vec4i {

                        int32(index % m_options.density.x),
                        int32((index % (m_options.density.x * m_options.density.y)) / m_options.density.x),
                        int32(index / (m_options.density.x * m_options.density.y)),
                        int32(index)
                    };

                    InitObject(envProbe);

                    m_envProbeCollection.AddProbe(index, envProbe);
                }
            }
        }
    }
}

void EnvGrid::SetAABB(const BoundingBox& aabb)
{
    HYP_SCOPE;

    if (m_aabb != aabb)
    {
        m_aabb = aabb;

        SetNeedsRenderProxyUpdate();
    }
}

void EnvGrid::Translate(const BoundingBox& aabb, const Vec3f& translation)
{
    HYP_SCOPE;
    AssertReady();

    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_aabb = aabb;

    const BoundingBox currentAabb = m_aabb;
    const Vec3f currentAabbCenter = currentAabb.GetCenter();
    const Vec3f currentAabbCenterMinusOffset = currentAabbCenter - m_offset;

    const Vec3f sizeOfProbe = SizeOfProbe();

    const Vec3i positionSnapped {
        MathUtil::Floor<float, Vec3i::Type>(translation.x / sizeOfProbe.x),
        MathUtil::Floor<float, Vec3i::Type>(translation.y / sizeOfProbe.y),
        MathUtil::Floor<float, Vec3i::Type>(translation.z / sizeOfProbe.z)
    };

    const Vec3i currentGridPosition {
        MathUtil::Floor<float, Vec3i::Type>(currentAabbCenterMinusOffset.x / sizeOfProbe.x),
        MathUtil::Floor<float, Vec3i::Type>(currentAabbCenterMinusOffset.y / sizeOfProbe.y),
        MathUtil::Floor<float, Vec3i::Type>(currentAabbCenterMinusOffset.z / sizeOfProbe.z)
    };

    m_aabb.SetCenter(Vec3f(positionSnapped) * sizeOfProbe + m_offset);

    if (currentGridPosition == positionSnapped)
    {
        return;
    }

    if (m_camera)
    {
        m_camera->SetTranslation(m_aabb.GetCenter());
    }

    Array<uint32> updates;
    updates.Resize(m_envProbeCollection.numProbes);

    for (uint32 x = 0; x < m_options.density.x; x++)
    {
        for (uint32 y = 0; y < m_options.density.y; y++)
        {
            for (uint32 z = 0; z < m_options.density.z; z++)
            {
                const Vec3i coord { int(x), int(y), int(z) };
                const Vec3i scrolledCoord = coord + positionSnapped;

                const Vec3i scrolledCoordClamped {
                    MathUtil::Mod(scrolledCoord.x, int(m_options.density.x)),
                    MathUtil::Mod(scrolledCoord.y, int(m_options.density.y)),
                    MathUtil::Mod(scrolledCoord.z, int(m_options.density.z))
                };

                const int scrolledClampedIndex = scrolledCoordClamped.x * m_options.density.x * m_options.density.y
                    + scrolledCoordClamped.y * m_options.density.x
                    + scrolledCoordClamped.z;

                const int index = x * m_options.density.x * m_options.density.y
                    + y * m_options.density.x
                    + z;

                Assert(scrolledClampedIndex >= 0);

                const BoundingBox newProbeAabb {
                    m_aabb.min + (Vec3f(float(x), float(y), float(z)) * sizeOfProbe),
                    m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * sizeOfProbe)
                };

                const Handle<EnvProbe>& probe = m_envProbeCollection.GetEnvProbeDirect(scrolledClampedIndex);

                if (!probe.IsValid())
                {
                    // Should not happen, but just in case
                    continue;
                }

                // If probe is at the edge of the grid, it will be moved to the opposite edge.
                // That means we need to re-render
                if (!probe->GetAABB().ContainsPoint(newProbeAabb.GetCenter()))
                {
                    probe->SetAABB(newProbeAabb);
                }

                updates[index] = scrolledClampedIndex;
            }
        }
    }

    for (uint32 updateIndex = 0; updateIndex < uint32(updates.Size()); updateIndex++)
    {
        Assert(updateIndex < m_envProbeCollection.numProbes);
        Assert(updates[updateIndex] < m_envProbeCollection.numProbes);

        m_envProbeCollection.SetIndexOnGameThread(updateIndex, updates[updateIndex]);
    }

    SetNeedsRenderProxyUpdate();
}

void EnvGrid::Update(float delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    static const ConfigurationValue& configDebugDrawProbes = GetGlobalConfig().Get("rendering.debug.debugDrawer.envGridProbes");

    // Debug draw
    if (configDebugDrawProbes.ToBool(false))
    {
        DebugDrawCommandList& debugDrawer = g_engineDriver->GetDebugDrawer()->CreateCommandList();

        for (uint32 index = 0; index < m_envProbeCollection.numProbes; index++)
        {
            const Handle<EnvProbe>& probe = m_envProbeCollection.GetEnvProbeDirect(index);

            if (!probe.IsValid())
            {
                continue;
            }

            debugDrawer.ambientProbe(probe->GetOrigin(), 0.25f, *probe);
        }
    }

    bool shouldRecollectEntites = false;

    if (!m_camera->IsReady())
    {
        shouldRecollectEntites = true;
    }

    BoundingBoxComponent* boundingBoxComponent = GetEntityManager()->TryGetComponent<BoundingBoxComponent>(this);
    if (!boundingBoxComponent)
    {
        HYP_LOG(EnvGrid, Error, "EnvGrid {} does not have a BoundingBoxComponent, cannot update", Id());
        return;
    }

    SceneOctree const* octree = &GetScene()->GetOctree();
    octree->GetFittingOctant(boundingBoxComponent->worldAabb, octree);

    // clang-format off
    const HashCode octantHashCode = octree->GetOctantID().GetHashCode()
        .Add(octree->GetEntryListHash<EntityTag::STATIC>())
        .Add(octree->GetEntryListHash<EntityTag::DYNAMIC>())
        .Add(octree->GetEntryListHash<EntityTag::LIGHT>());
    // clang-format on

    if (octantHashCode != m_cachedOctantHashCode)
    {
        HYP_LOG(EnvGrid, Debug, "EnvGrid octant hash code changed ({} != {}), updating probes", m_cachedOctantHashCode.Value(), octantHashCode.Value());

        m_cachedOctantHashCode = octantHashCode;

        shouldRecollectEntites = true;
    }

    if (!shouldRecollectEntites)
    {
        return;
    }

    for (uint32 index = 0; index < m_envProbeCollection.numProbes; index++)
    {
        const Handle<EnvProbe>& probe = m_envProbeCollection.GetEnvProbeDirect(index);
        Assert(probe.IsValid());

        // so Collect() on our view updates the EnvProbe's RenderProxy
        probe->SetNeedsRenderProxyUpdate();
        probe->SetNeedsRender(true);
    }

    m_camera->Update(delta);

    m_view->UpdateViewport();
    m_view->UpdateVisibility();
    m_view->CollectSync();

    HYP_LOG(EnvGrid, Debug, "View::Collect() for EnvGrid {}", Id());

    // Make sure all our probes were collected - if this doesn't match up, down the line when we try to retrieve resource binding indices, they wouldn't be found
    RenderProxyList& rpl = RenderApi_GetProducerProxyList(m_view);
    AssertDebug(rpl.GetEnvProbes().NumCurrent() >= m_envProbeCollection.numProbes,
        "View only collected {} EnvProbes but EnvGrid {} has {} EnvProbes",
        rpl.GetEnvProbes().NumCurrent(),
        Id(),
        m_envProbeCollection.numProbes);

    HYP_LOG(EnvGrid, Debug, "Updating EnvGrid {} with {} probes\t Found {} meshes", Id(), m_envProbeCollection.numProbes,
        RenderApi_GetProducerProxyList(m_view).GetMeshEntities().NumCurrent());
}

void EnvGrid::UpdateRenderProxy(RenderProxyEnvGrid* proxy)
{
    proxy->envGrid = WeakHandleFromThis();

    EnvGridShaderData& bufferData = proxy->bufferData;
    bufferData.center = Vec4f(m_aabb.GetCenter(), 1.0f);
    bufferData.extent = Vec4f(m_aabb.GetExtent(), 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.density = { m_options.density.x, m_options.density.y, m_options.density.z, 0 };
    bufferData.voxelGridAabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.voxelGridAabbMin = Vec4f(m_aabb.min, 1.0f);

    bufferData.lightFieldImageDimensions = m_irradianceTexture.IsValid()
        ? Vec2i(m_irradianceTexture->GetExtent().GetXY())
        : Vec2i::Zero();

    bufferData.irradianceOctahedronSize = Vec2i(irradianceOctahedronSize);

    Memory::MemSet(&proxy->envProbes[0], 0, sizeof(proxy->envProbes));

    for (uint32 index = 0; index < m_envProbeCollection.numProbes; index++)
    {
        const Handle<EnvProbe>& probe = m_envProbeCollection.GetEnvProbeOnGameThread(index);
        Assert(probe.IsValid());

        proxy->envProbes[index] = probe->Id();
    }
}

#pragma endregion EnvGrid

} // namespace hyperion
