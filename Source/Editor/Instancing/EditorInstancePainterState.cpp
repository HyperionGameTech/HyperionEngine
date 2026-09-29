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

#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/Ray.hpp>
#include <Core/Math/MathUtil.hpp>

#include <EditorInstancePainterState.generated.inl>

namespace Hyperion {

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
            m_prefabBounds = BoundingBox::Empty();

            if (!m_activePrefab.IsValid())
            {
                return;
            }

            Array<InstanceGroupMember> members;
            InstanceGroup::CollectMembers(*m_activePrefab, members);

            for (const InstanceGroupMember& member : members)
            {
                m_prefabBounds = m_prefabBounds.Union(member.bounds);
            }
        });
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
    return m_activePrefab.IsValid();
}

Vec3f EditorInstancePainterState::GetPlacementScale() const
{
    return Vec3f(m_scale);
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
    Entity* entity = DynamicCast<Entity>(hit.node);

    const InstanceClusterComponent* clusterComponent = entity != nullptr && entity->GetEntityManager() != nullptr
        ? entity->TryGetComponent<InstanceClusterComponent>()
        : nullptr;

    if (!clusterComponent)
    {
        return false;
    }

    // otherwise a stroke would keep stacking instances on top of the ones it just placed
    Handle<InstanceGroup> group = clusterComponent->group.Lock();

    return group.IsValid() && group->GetPrefab() == m_activePrefab;
}

Vec3f EditorInstancePainterState::GetPlacementPosition(const SurfaceHit& hit) const
{
    static constexpr uint32 numProbes = 8;

    float groundHeight = hit.position.y;

    // an upright instance on a slope floats on its downhill side, so drop it to the lowest ground under its footprint.
    // Aligned to the surface, its base already follows the slope
    if (!m_alignToSurface && m_footprintRadius > 0.0f)
    {
        const float radius = m_footprintRadius * m_scale;

        // probes that fall off a ledge or into a hole shouldn't drag it down with them
        const float lowestAllowed = hit.position.y - radius * 3.0f;

        for (uint32 probeIndex = 0; probeIndex < numProbes; probeIndex++)
        {
            const float angle = float(probeIndex) * (2.0f * MathUtil::pi<float> / float(numProbes));
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

    return Vec3f(hit.position.x, groundHeight - m_sinkDepth * m_scale, hit.position.z);
}

Handle<InstanceGroup> EditorInstancePainterState::FindOrCreateInstanceGroup(const Handle<Scene>& scene)
{
    AssertOnThread(g_simThread);

    for (const auto& edit : m_stroke.GetEdits())
    {
        if (edit.target->GetPrefab() == m_activePrefab)
        {
            return edit.target;
        }
    }

    Handle<InstanceGroup> group = InstanceGroup::Find(scene.Get(), m_activePrefab);

    if (group.IsValid())
    {
        return group;
    }

    group = InstanceGroup::Create(scene.Get(), m_activePrefab);

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

    if (!m_activePrefab.IsValid() || !AcceptStrokeScene(hit.scene))
    {
        return;
    }

    Handle<InstanceGroup> group = FindOrCreateInstanceGroup(hit.scene);

    if (!group.IsValid())
    {
        return;
    }

    const Transform transform = MakePlacementTransform(hit);
    const InstanceId id = group->AddInstance(transform);

    m_stroke.RecordAdded(group, InstanceRecord::FromTransform(id, transform));

    OnStamped(hit);
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
