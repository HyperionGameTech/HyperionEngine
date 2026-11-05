/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Instancing/EditorInstancePainterState.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Instancing/InstanceGroup.hpp>
#include <Scene/Components/InstanceClusterComponent.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Ray.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorInstancePainterState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorInstancePainterState

EditorInstancePainterState::EditorInstancePainterState()
    : m_prefabBounds(BoundingBox::Empty())
{
    m_spacing = 2.0f;
    m_eraseRadius = 2.0f;
    m_alignToSurface = false;
}

EditorInstancePainterState::~EditorInstancePainterState() = default;

const Handle<Prefab>& EditorInstancePainterState::GetActivePrefab() const
{
    return m_activePrefab;
}

void EditorInstancePainterState::SetActivePrefab(const Handle<Prefab>& prefab)
{
    DispatchToSimThread([this, prefab]()
        {
            AssertOnThread(g_simThread);

            m_activePrefab = prefab;

            if (prefab.IsValid())
            {
                m_activePrefabSet.Reset();
            }

            DrawNextStamp();
        });
}

const Handle<PrefabSet>& EditorInstancePainterState::GetActivePrefabSet() const
{
    return m_activePrefabSet;
}

void EditorInstancePainterState::SetActivePrefabSet(const Handle<PrefabSet>& prefabSet)
{
    DispatchToSimThread([this, prefabSet]()
        {
            AssertOnThread(g_simThread);

            if (prefabSet != m_activePrefabSet)
            {
                m_excludedPrefabs.Clear();
            }

            m_activePrefabSet = prefabSet;

            if (prefabSet.IsValid())
            {
                m_activePrefab.Reset();
            }

            DrawNextStamp();
        });
}

void EditorInstancePainterState::SetActivePrefabByName(Name assetName)
{
    DispatchToSimThread([this, assetName]()
        {
            Handle<AssetRegistry> registry = GetCurrentAssetRegistry();
            Handle<Prefab> prefab = registry ? TryCast<Prefab>(registry->GetAsset(AssetBuckets::Prefabs, assetName)) : Handle<Prefab>::Null();

            if (!prefab.IsValid())
            {
                HYP_LOG(Editor, Warning, "'{}' is not a Prefab asset", assetName);

                return;
            }

            SetActivePrefab(prefab);
        });
}

void EditorInstancePainterState::SetActivePrefabSetByName(Name assetName)
{
    DispatchToSimThread([this, assetName]()
        {
            Handle<AssetRegistry> registry = GetCurrentAssetRegistry();
            Handle<PrefabSet> prefabSet = registry ? TryCast<PrefabSet>(registry->GetAsset(AssetBuckets::Misc, assetName)) : Handle<PrefabSet>::Null();

            if (!prefabSet.IsValid())
            {
                HYP_LOG(Editor, Warning, "'{}' is not a PrefabSet asset", assetName);

                return;
            }

            SetActivePrefabSet(prefabSet);
        });
}

Array<PrefabSetEntry> EditorInstancePainterState::GetPalette() const
{
    if (m_activePrefabSet.IsValid())
    {
        return m_activePrefabSet->GetValidEntries();
    }

    Array<PrefabSetEntry> palette;

    if (m_activePrefab.IsValid())
    {
        PrefabSetEntry entry;
        entry.prefab = m_activePrefab;

        palette.PushBack(std::move(entry));
    }

    return palette;
}

bool EditorInstancePainterState::IsExcluded(const Handle<Prefab>& prefab) const
{
    for (const WeakHandle<Prefab>& excluded : m_excludedPrefabs)
    {
        if (excluded.GetUnsafe() == prefab.Get())
        {
            return true;
        }
    }

    return false;
}

uint32 EditorInstancePainterState::GetPaletteCount() const
{
    return uint32(GetPalette().Size());
}

Name EditorInstancePainterState::GetPaletteName(uint32 index) const
{
    const Array<PrefabSetEntry> palette = GetPalette();

    return index < palette.Size() ? palette[index].prefab->GetName() : Name::Invalid();
}

float EditorInstancePainterState::GetPaletteWeight(uint32 index) const
{
    const Array<PrefabSetEntry> palette = GetPalette();

    return index < palette.Size() ? palette[index].weight : 0.0f;
}

bool EditorInstancePainterState::IsPaletteEntryIncluded(uint32 index) const
{
    const Array<PrefabSetEntry> palette = GetPalette();

    return index < palette.Size() && !IsExcluded(palette[index].prefab);
}

void EditorInstancePainterState::SetPaletteEntryIncluded(uint32 index, bool included)
{
    DispatchToSimThread([this, index, included]()
        {
            const Array<PrefabSetEntry> palette = GetPalette();

            if (index >= palette.Size() || included == !IsExcluded(palette[index].prefab))
            {
                return;
            }

            if (included)
            {
                m_excludedPrefabs.RemoveAll([&palette, index](const WeakHandle<Prefab>& excluded)
                    {
                        return excluded.GetUnsafe() == palette[index].prefab.Get();
                    });
            }
            else
            {
                m_excludedPrefabs.PushBack(MakeWeakRef(palette[index].prefab));
            }

            DrawNextStamp();
        });
}

float EditorInstancePainterState::GetScaleMin() const
{
    return m_scaleMin;
}

void EditorInstancePainterState::SetScaleMin(float scaleMin)
{
    DispatchToSimThread([this, scaleMin]()
        {
            m_scaleMin = MathUtil::Max(scaleMin, 0.01f);

            DrawNextStamp();
        });
}

float EditorInstancePainterState::GetScaleMax() const
{
    return m_scaleMax;
}

void EditorInstancePainterState::SetScaleMax(float scaleMax)
{
    DispatchToSimThread([this, scaleMax]()
        {
            m_scaleMax = MathUtil::Max(scaleMax, 0.01f);

            DrawNextStamp();
        });
}

void EditorInstancePainterState::DrawNextStamp()
{
    AssertOnThread(g_simThread);

    m_nextPrefab.Reset();

    if (m_activePrefabSet.IsValid())
    {
        const Array<PrefabSetEntry> palette = GetPalette();

        float totalWeight = 0.0f;

        for (const PrefabSetEntry& entry : palette)
        {
            if (IsExcluded(entry.prefab))
            {
                continue;
            }

            totalWeight += entry.weight;
        }

        float remaining = RandomInRange(0.0f, totalWeight);

        for (const PrefabSetEntry& entry : palette)
        {
            if (IsExcluded(entry.prefab))
            {
                continue;
            }

            m_nextPrefab = entry.prefab;

            if (remaining < entry.weight)
            {
                break;
            }

            remaining -= entry.weight;
        }
    }
    else
    {
        m_nextPrefab = m_activePrefab;
    }

    const float scaleLow = MathUtil::Min(m_scaleMin, m_scaleMax);
    const float scaleHigh = MathUtil::Max(m_scaleMin, m_scaleMax);

    m_nextScale = scaleHigh > scaleLow ? RandomInRange(scaleLow, scaleHigh) : scaleLow;

    m_prefabBounds = BoundingBox::Empty();

    if (!m_nextPrefab.IsValid())
    {
        return;
    }

    Array<InstanceGroupMember> members;
    InstanceGroup::CollectMembers(*m_nextPrefab, members);

    for (const InstanceGroupMember& member : members)
    {
        m_prefabBounds = m_prefabBounds.Union(member.bounds);
    }
}

bool EditorInstancePainterState::IsPaintedPrefab(const Handle<Prefab>& prefab) const
{
    if (!prefab.IsValid())
    {
        return false;
    }

    if (prefab == m_activePrefab)
    {
        return true;
    }

    if (m_activePrefabSet.IsValid())
    {
        for (const PrefabSetEntry& entry : m_activePrefabSet->GetEntries())
        {
            if (entry.prefab == prefab)
            {
                return true;
            }
        }
    }

    return false;
}

float EditorInstancePainterState::GetFootprintRadius() const
{
    return m_footprintRadius;
}

void EditorInstancePainterState::SetFootprintRadius(float footprintRadius)
{
    DispatchToSimThread([this, footprintRadius]()
        {
            m_footprintRadius = MathUtil::Max(footprintRadius, 0.0f);
        });
}

float EditorInstancePainterState::GetSinkDepth() const
{
    return m_sinkDepth;
}

void EditorInstancePainterState::SetSinkDepth(float sinkDepth)
{
    DispatchToSimThread([this, sinkDepth]()
        {
            m_sinkDepth = sinkDepth;
        });
}

bool EditorInstancePainterState::HasActiveAsset() const
{
    for (const PrefabSetEntry& entry : GetPalette())
    {
        if (!IsExcluded(entry.prefab))
        {
            return true;
        }
    }

    return false;
}

Vec3f EditorInstancePainterState::GetPlacementScale() const
{
    return Vec3f(m_scale * m_nextScale);
}

Transform EditorInstancePainterState::GetCursorTransform(const SurfaceHit& hit) const
{
    // a prefab with nothing instanceable still gets a 1m box so the cursor shows where it would go
    const BoundingBox bounds = m_prefabBounds.IsValid()
        ? m_prefabBounds
        : BoundingBox(Vec3f(-0.5f, 0.0f, -0.5f), Vec3f(0.5f, 1.0f, 0.5f));

    return MakePlacementTransform(hit) * Transform(bounds.GetCenter(), bounds.GetExtent() * 0.5f, Quat4f::Identity());
}

bool EditorInstancePainterState::ShouldIgnoreHit(const RayHit& hit) const
{
    Entity* entity = TryCast<Entity>(hit.node);

    const InstanceClusterComponent* clusterComponent = entity != nullptr && entity->GetEntityManager() != nullptr
        ? entity->TryGetComponent<InstanceClusterComponent>()
        : nullptr;

    if (!clusterComponent)
    {
        return false;
    }

    // otherwise a stroke would keep stacking instances on top of the ones it just placed
    Handle<InstanceGroup> group = clusterComponent->group.Lock();

    return group.IsValid() && IsPaintedPrefab(group->GetPrefab());
}

Vec3f EditorInstancePainterState::GetPlacementPosition(const SurfaceHit& hit) const
{
    static constexpr uint32 NumPlacementProbes = 8;

    float groundHeight = hit.position.y;

    if (!m_alignToSurface && m_footprintRadius > 0.0f)
    {
        const float radius = m_footprintRadius * GetPlacementScale().x;

        // probes that fall off a ledge or into a hole shouldn't drag it down with them
        const float lowestAllowed = hit.position.y - radius * 3.0f;

        for (uint32 probeIndex = 0; probeIndex < NumPlacementProbes; probeIndex++)
        {
            const float angle = float(probeIndex) * (2.0f * MathUtil::pi<float> / float(NumPlacementProbes));
            const Vec3f probePosition = hit.position + Vec3f(MathUtil::Cos(angle) * radius, 0.0f, MathUtil::Sin(angle) * radius);

            Ray ray;
            ray.position = probePosition + Vec3f(0.0f, radius * 3.0f + 1.0f, 0.0f);
            ray.direction = -Vec3f::UnitY();

            SurfaceHit probeHit;

            if (RaycastSurface(ray, probeHit) && probeHit.position.y >= lowestAllowed)
            {
                groundHeight = MathUtil::Min(groundHeight, probeHit.position.y);
            }
        }
    }

    return Vec3f(hit.position.x, groundHeight - m_sinkDepth * GetPlacementScale().x, hit.position.z);
}

Handle<InstanceGroup> EditorInstancePainterState::FindOrCreateInstanceGroup(const Handle<Scene>& scene, const Handle<Prefab>& prefab)
{
    AssertOnThread(g_simThread);

    for (const auto& edit : m_stroke.GetEdits())
    {
        if (edit.target->GetPrefab() == prefab)
        {
            return edit.target;
        }
    }

    Handle<InstanceGroup> group = InstanceGroup::Find(scene.Get(), prefab);

    if (group.IsValid())
    {
        return group;
    }

    group = InstanceGroup::Create(scene.Get(), prefab);

    if (!group.IsValid())
    {
        return group;
    }

    scene->GetRoot()->AddChild(group);

    m_stroke.GetEdit(group, /* createdByStroke */ true);

    return group;
}

void EditorInstancePainterState::StampAt(const SurfaceHit& hit)
{
    AssertOnThread(g_simThread);

    if (!m_nextPrefab.IsValid() || !IsPaintedPrefab(m_nextPrefab))
    {
        DrawNextStamp();
    }

    if (!m_nextPrefab.IsValid() || !AcceptStrokeScene(hit.scene))
    {
        return;
    }

    Handle<InstanceGroup> group = FindOrCreateInstanceGroup(hit.scene, m_nextPrefab);

    if (!group.IsValid())
    {
        return;
    }

    const Transform transform = MakePlacementTransform(hit);
    const InstanceId id = group->AddInstance(transform);

    m_stroke.RecordAdded(group, InstanceRecord::FromTransform(id, transform));

    OnStamped(hit);
    DrawNextStamp();
}

void EditorInstancePainterState::EraseAt(const SurfaceHit& hit)
{
    AssertOnThread(g_simThread);

    if (!AcceptStrokeScene(hit.scene))
    {
        return;
    }

    Array<Handle<InstanceGroup>> groups;

    for (auto [group] : hit.scene->GetEntityManager()->GetEntitySet<EntityType<InstanceGroup>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        groups.PushBack(MakeStrongRef(group));
    }

    const BoundingSphere sphere(hit.position, m_eraseRadius);

    Array<InstanceRecord> removed;

    for (const Handle<InstanceGroup>& group : groups)
    {
        removed.Resize(0); // clear, but don't free memory

        group->RemoveInstancesInSphere(sphere, removed);

        for (const InstanceRecord& record : removed)
        {
            m_stroke.RecordRemoved(group, record);
        }
    }
}

void EditorInstancePainterState::ResetStrokeEdits()
{
    m_stroke.Reset();
}

void EditorInstancePainterState::CommitStrokeEdits(const WeakHandle<Scene>& strokeScene)
{
    m_stroke.Commit(m_subsystem, strokeScene);
}

#pragma endregion EditorInstancePainterState

} // namespace Hyperion
