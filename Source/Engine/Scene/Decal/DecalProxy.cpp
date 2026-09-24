/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Decal/DecalProxy.hpp>
#include <Scene/Decal/Decal.hpp>
#include <Scene/Scene.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Math/MathUtil.hpp>

#include <DecalProxy.generated.inl>

namespace Hyperion {

static const BoundingBox s_unitDecalBox = BoundingBox(Vec3f(-1.0f), Vec3f(1.0f));

static bool DecalProjectionBoxIntersectsSphere(const Transform& transform, const BoundingSphere& sphere)
{
    const Mat4f matrix = transform.GetMatrix();

    const Vec3f boxCenter = matrix.ExtractTranslation();
    const Vec3f offset = sphere.center - boxCenter;

    Vec3f closestPoint = boxCenter;

    for (uint32 axisIndex = 0; axisIndex < 3; axisIndex++)
    {
        // columns hold the rotated axes scaled by the half extent
        const Vec3f axis = matrix.GetColumn(axisIndex).GetXYZ();
        const float axisLengthSquared = axis.LengthSquared();

        if (axisLengthSquared <= MathUtil::epsilonF)
        {
            continue;
        }

        const float projection = MathUtil::Clamp(offset.Dot(axis) / axisLengthSquared, -1.0f, 1.0f);

        closestPoint += axis * projection;
    }

    return closestPoint.DistanceSquared(sphere.center) <= sphere.radius * sphere.radius;
}

DecalProxy::DecalProxy()
    : m_nextDecalId(0),
      m_decalBounds(BoundingBox::Empty()),
      m_cachedDecal(nullptr),
      m_cachedDecalVersion(-1),
      m_cachedMaterial(nullptr),
      m_cachedMaterialVersion(-1)
{
    // Update() gets called each tick, to update decal render proxy on cache stamp mismatches
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = true;
}

DecalProxy::DecalProxy(const Handle<Decal>& decal)
    : DecalProxy()
{
    m_decal = decal;
}

DecalProxy::~DecalProxy() = default;

void DecalProxy::Init()
{
    Entity::Init();

    // placements are stored in world space
    LockTransform();

    UpdateDecalBounds();
}

void DecalProxy::SetDecal(const Handle<Decal>& decal)
{
    if (m_decal == decal)
    {
        return;
    }

    m_decal = decal;

    SetNeedsRenderProxyUpdate();
}

DecalId DecalProxy::AddDecal(const Transform& transform)
{
    DecalInstance instance;
    instance.id = DecalId(m_nextDecalId++);
    instance.transform = transform;

    AssertDebug(instance.id != Invalid<DecalId>, "Out of decal ids");

    m_instances.PushBack(instance);

    OnDecalsChanged();

    return instance.id;
}

bool DecalProxy::AddDecalWithId(const DecalInstance& instance)
{
    if (instance.id == Invalid<DecalId>)
    {
        return false;
    }

    const bool exists = m_instances.FindIf([&instance](const DecalInstance& existing)
                                      {
                                          return existing.id == instance.id;
                                      })
        != m_instances.End();

    if (exists)
    {
        return false;
    }

    m_instances.PushBack(instance);

    m_nextDecalId = MathUtil::Max(m_nextDecalId, uint32(instance.id) + 1);

    OnDecalsChanged();

    return true;
}

bool DecalProxy::RemoveDecal(DecalId id)
{
    auto it = m_instances.FindIf([id](const DecalInstance& instance)
        {
            return instance.id == id;
        });

    if (it == m_instances.End())
    {
        return false;
    }

    m_instances.Erase(it);

    OnDecalsChanged();

    return true;
}

void DecalProxy::RemoveDecalsInSphere(
    const BoundingSphere& bounds,
    Array<DecalInstance, SceneAllocator>& outRemovedDecalInstances)
{
    uint32 numRemoved = 0;

    for (auto it = m_instances.Begin(); it != m_instances.End();)
    {
        if (!DecalProjectionBoxIntersectsSphere(it->transform, bounds))
        {
            ++it;

            continue;
        }

        outRemovedDecalInstances.PushBack(*it);
        ++numRemoved;

        it = m_instances.Erase(it);
    }

    if (numRemoved)
    {
        OnDecalsChanged();
    }
}

void DecalProxy::ClearDecals()
{
    if (m_instances.Empty())
    {
        return;
    }

    m_instances.Clear();

    OnDecalsChanged();
}

void DecalProxy::OnDecalsChanged()
{
    UpdateDecalBounds();

    if (Scene* scene = GetScene())
    {
        scene->MarkDirty();
    }
}

void DecalProxy::UpdateDecalBounds()
{
    m_decalBounds = BoundingBox::Empty();

    for (const DecalInstance& instance : m_instances)
    {
        m_decalBounds = m_decalBounds.Union(instance.transform.GetMatrix() * s_unitDecalBox);
    }

    // the proxy sits at the origin with a locked transform, so local bounds are world bounds (editor picking + octree).
    // zero box rather than Empty() when there are no decals: VisThread drops invalid bounds without leaving the octree, so re-inserting later would fail
    SetLocalBounds(m_decalBounds.IsValid() ? m_decalBounds : BoundingBox::Zero());

    SetNeedsRenderProxyUpdate();
}

void DecalProxy::Update(float delta)
{
    Decal* decal = m_decal.Get();
    const int decalVersion = decal != nullptr ? *decal->GetRenderProxyVersionPtr() : -1;

    Material* material = decal != nullptr ? decal->GetMaterial().Get() : nullptr;
    const int materialVersion = material != nullptr ? *material->GetRenderProxyVersionPtr() : -1;

    if (decal == m_cachedDecal
        && decalVersion == m_cachedDecalVersion
        && material == m_cachedMaterial
        && materialVersion == m_cachedMaterialVersion)
    {
        return;
    }

    m_cachedDecal = decal;
    m_cachedDecalVersion = decalVersion;
    m_cachedMaterial = material;
    m_cachedMaterialVersion = materialVersion;

    SetNeedsRenderProxyUpdate();
}

void DecalProxy::UpdateRenderProxy(RenderProxyDecalProxy* proxy)
{
    AssertDebug(proxy != nullptr);

    proxy->decalProxy = this;
    proxy->worldAabb = m_decalBounds;

    proxy->instances.Resize(m_instances.Size());

    for (uint32 index = 0; index < uint32(m_instances.Size()); index++)
    {
        DecalInstanceShaderData& shaderData = proxy->instances[index];

        // @TODO swap to decalToWorld instead!!
        shaderData.worldToDecal = m_instances[index].transform.GetMatrix().Inverse();
    }

    proxy->albedoTexture = nullptr;
    proxy->normalTexture = nullptr;
    proxy->sortOrder = 0;
    proxy->bufferData = {};

    if (!m_decal.IsValid())
    {
        return;
    }

    const DecalDesc& desc = m_decal->GetDecalDesc();
    Material* material = m_decal->GetMaterial().Get();

    if (material != nullptr)
    {
        proxy->albedoTexture = material->GetTexture(MaterialTextureKey::Diffuse).Get();
        proxy->normalTexture = material->GetTexture(MaterialTextureKey::Normals).Get();
    }

    proxy->sortOrder = desc.sortOrder;

    DecalTypeShaderData& bufferData = proxy->bufferData;
    bufferData.tint = material != nullptr ? Vec4f(material->GetParameters().albedo) : Vec4f(1.0f);
    bufferData.opacity = desc.opacity;
    bufferData.normalStrength = desc.normalStrength;
    bufferData.angleFadeStart = desc.angleFadeStart;
    bufferData.angleFadeEnd = desc.angleFadeEnd;
    bufferData.excludeMask = uint32(desc.excludeMask);
    bufferData.flags = DTF_NONE;

    if (proxy->albedoTexture != nullptr)
    {
        bufferData.flags |= DTF_HAS_ALBEDO_MAP;
    }

    if (proxy->normalTexture != nullptr)
    {
        bufferData.flags |= DTF_HAS_NORMAL_MAP;

        if (material->GetParameters().flags & MaterialParameters::FlagBit_NormalMapFlipY)
        {
            bufferData.flags |= DTF_NORMAL_MAP_FLIP_Y;
        }
    }
}

} // namespace Hyperion
