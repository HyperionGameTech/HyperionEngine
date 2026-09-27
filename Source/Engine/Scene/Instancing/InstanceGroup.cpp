/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Instancing/InstanceGroup.hpp>
#include <Scene/Instancing/InstanceSetData.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/InstanceClusterComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/EngineGlobals.hpp>

#include <InstanceGroup.generated.inl>

namespace Hyperion {

static constexpr float MinClusterSize = 1.0f;

InstanceGroup::InstanceGroup()
    : m_clusterSize(64.0f),
      m_nextId(0),
      m_prefabBounds(BoundingBox::Empty()),
      m_revision(0),
      m_instancesLoaded(false),
      m_instancesNeedStore(false),
      m_needsFullRebuild(true)
{
    // draws nothing itself, its cluster entities are what get culled and picked
    m_nodeFlags |= NodeFlags::ExcludeFromOctree;
}

InstanceGroup::InstanceGroup(const Handle<Prefab>& prefab)
    : InstanceGroup()
{
    m_prefab = prefab;
}

InstanceGroup::~InstanceGroup() = default;

void InstanceGroup::Init()
{
    Entity::Init();

    // instance transforms are stored in world space
    if (GetLocalTransform() != Transform::identity)
    {
        SetLocalTransform(Transform::identity);
    }

    LockTransform();
}

Handle<Node> InstanceGroup::Clone() const
{
    Handle<Node> cloned = Entity::Clone();

    InstanceGroup* clonedGroup = DynamicCast<InstanceGroup>(cloned.Get());

    if (!clonedGroup)
    {
        return cloned;
    }

    // the placements belong to this group, the copy gets its own (including edits not yet stored)
    Handle<InstanceSetData> instanceData;

    if (m_instancesLoaded)
    {
        Array<InstanceRecord> records;
        records.Reserve(m_instances.Size());

        for (const InstanceEntry& entry : m_instances)
        {
            records.PushBack(InstanceRecord::FromTransform(entry.id, entry.transform));
        }

        instanceData = MakeHandle<InstanceSetData>(clonedGroup->GetName());
        instanceData->SetRecords(Span<const InstanceRecord>(records.Data(), records.Size()), m_nextId);
    }
    else if (m_instanceData.IsValid())
    {
        instanceData = DynamicCast<InstanceSetData>(m_instanceData->CloneAsset());
    }

    if (!instanceData.IsValid())
    {
        instanceData = MakeHandle<InstanceSetData>(clonedGroup->GetName());
    }

    if (m_instanceData.IsValid() && m_instanceData->IsRegistered())
    {
        GetCurrentAssetRegistry()->PutAssetUnique(instanceData);
    }

    clonedGroup->SetInstanceData(instanceData);

    return cloned;
}

void InstanceGroup::SetPrefab(const Handle<Prefab>& prefab)
{
    if (m_prefab == prefab)
    {
        return;
    }

    m_prefab = prefab;

    MarkAllCellsDirty();
}

void InstanceGroup::SetInstanceData(const Handle<InstanceSetData>& instanceData)
{
    if (m_instanceData == instanceData)
    {
        return;
    }

    m_instanceData = instanceData;

    m_instances.Clear();
    m_instanceIndices.Clear();
    m_nextId = 0;

    m_instancesLoaded = false;
    m_instancesNeedStore = false;

    MarkAllCellsDirty();
}

#ifdef HYP_EDITOR
void InstanceGroup::RebuildInstances()
{
    MarkAllCellsDirty();
}
#endif

void InstanceGroup::SetClusterSize(float clusterSize)
{
    clusterSize = MathUtil::Max(clusterSize, MinClusterSize);

    if (clusterSize == m_clusterSize)
    {
        return;
    }

    m_clusterSize = clusterSize;

    MarkAllCellsDirty();
}

Vec2i InstanceGroup::GetCell(const Vec3f& position) const
{
    const float clusterSize = MathUtil::Max(m_clusterSize, MinClusterSize);

    return Vec2i(MathUtil::Floor(position.x / clusterSize), MathUtil::Floor(position.z / clusterSize));
}

void InstanceGroup::EnsureInstancesLoaded()
{
    if (m_instancesLoaded)
    {
        return;
    }

    m_instancesLoaded = true;

    m_instances.Clear();
    m_instanceIndices.Clear();
    m_nextId = 0;

    if (!m_instanceData.IsValid())
    {
        return;
    }

    Array<InstanceRecord> records;
    m_instanceData->GetRecords(records);

    m_nextId = m_instanceData->GetNextId();

    m_instances.Reserve(records.Size());

    for (const InstanceRecord& record : records)
    {
        const InstanceId id = record.GetId();

        if (id == InvalidInstanceId || m_instanceIndices.Contains(uint32(id)))
        {
            HYP_LOG(Scene, Warning, "InstanceGroup '{}': skipping saved instance with a duplicate or invalid id {}", GetName(), record.id);

            continue;
        }

        const Transform transform = record.GetTransform();

        m_instanceIndices.Set(uint32(id), uint32(m_instances.Size()));
        m_instances.PushBack(InstanceEntry { id, transform, GetCell(transform.GetTranslation()) });

        m_nextId = MathUtil::Max(m_nextId, uint32(id) + 1);
    }
}

void InstanceGroup::StoreInstances()
{
    AssertOnThread(g_simThread);

    if (!m_instanceData.IsValid())
    {
        m_instanceData = MakeHandle<InstanceSetData>(GetName());
        GetCurrentAssetRegistry()->PutAssetUnique(m_instanceData);
    }

    Array<InstanceRecord> records;
    records.Reserve(m_instances.Size());

    for (const InstanceEntry& entry : m_instances)
    {
        records.PushBack(InstanceRecord::FromTransform(entry.id, entry.transform));
    }

    m_instanceData->SetRecords(records.ToSpan(), m_nextId);
}

InstanceId InstanceGroup::AddInstance(const Transform& transform)
{
    EnsureInstancesLoaded();

    const InstanceId id = InstanceId(m_nextId);
    AssertDebug(id != InvalidInstanceId, "Out of instance ids");

    AddInstanceWithId(id, transform);

    return id;
}

bool InstanceGroup::AddInstanceWithId(InstanceId id, const Transform& transform)
{
    EnsureInstancesLoaded();

    if (id == InvalidInstanceId || m_instanceIndices.Contains(uint32(id)))
    {
        return false;
    }

    const Vec2i cell = GetCell(transform.GetTranslation());

    m_instanceIndices.Set(uint32(id), uint32(m_instances.Size()));
    m_instances.PushBack(InstanceEntry { id, transform, cell });

    m_nextId = MathUtil::Max(m_nextId, uint32(id) + 1);

    AddToCell(id, cell);

    OnInstancesChanged();

    return true;
}

bool InstanceGroup::RemoveInstance(InstanceId id)
{
    EnsureInstancesLoaded();

    const auto indexIt = m_instanceIndices.Find(uint32(id));

    if (indexIt == m_instanceIndices.End())
    {
        return false;
    }

    const uint32 index = indexIt->second;
    m_instanceIndices.Erase(indexIt);

    RemoveFromCell(id, m_instances[index].cell);

    // swap with the last so the other indices stay valid
    const uint32 lastIndex = uint32(m_instances.Size()) - 1;

    if (index != lastIndex)
    {
        m_instances[index] = m_instances[lastIndex];
        m_instanceIndices.Set(uint32(m_instances[index].id), index);
    }

    m_instances.PopBack();

    OnInstancesChanged();

    return true;
}

bool InstanceGroup::SetInstanceTransform(InstanceId id, const Transform& transform)
{
    EnsureInstancesLoaded();

    const auto indexIt = m_instanceIndices.Find(uint32(id));

    if (indexIt == m_instanceIndices.End())
    {
        return false;
    }

    InstanceEntry& entry = m_instances[indexIt->second];

    if (entry.transform == transform)
    {
        return true;
    }

    const Vec2i cell = GetCell(transform.GetTranslation());

    if (cell != entry.cell)
    {
        RemoveFromCell(id, entry.cell);
        AddToCell(id, cell);

        entry.cell = cell;
    }
    else
    {
        MarkCellDirty(cell);
    }

    entry.transform = transform;

    OnInstancesChanged();

    return true;
}

bool InstanceGroup::GetInstanceTransform(InstanceId id, Transform& outTransform) const
{
    const auto indexIt = m_instanceIndices.Find(uint32(id));

    if (indexIt == m_instanceIndices.End())
    {
        return false;
    }

    outTransform = m_instances[indexIt->second].transform;

    return true;
}

InstanceId InstanceGroup::GetClusterInstanceId(Vec2i cell, uint32 instanceIndex) const
{
    const auto clusterIt = m_clusters.Find(cell);

    if (clusterIt == m_clusters.End() || instanceIndex >= clusterIt->second.instanceIds.Size())
    {
        return InvalidInstanceId;
    }

    return clusterIt->second.instanceIds[instanceIndex];
}

void InstanceGroup::OnInstancesChanged()
{
    ++m_revision;

    m_instancesNeedStore = true;

    if (GetEntityManager() != nullptr)
    {
        AddTag<EntityTag::UpdateInstanceGroup>();
    }

    if (Scene* scene = GetScene())
    {
        scene->MarkDirty();
    }
}

void InstanceGroup::MarkCellDirty(Vec2i cell)
{
    const auto clusterIt = m_clusters.Find(cell);

    if (clusterIt != m_clusters.End())
    {
        clusterIt->second.isDirty = true;
    }

    if (GetEntityManager() != nullptr)
    {
        AddTag<EntityTag::UpdateInstanceGroup>();
    }
}

void InstanceGroup::MarkAllCellsDirty()
{
    m_needsFullRebuild = true;

    if (GetEntityManager() != nullptr)
    {
        AddTag<EntityTag::UpdateInstanceGroup>();
    }
}

void InstanceGroup::AddToCell(InstanceId id, Vec2i cell)
{
    // cells are rebucketed from scratch on a full rebuild
    if (m_needsFullRebuild)
    {
        return;
    }

    auto clusterIt = m_clusters.Find(cell);

    if (clusterIt == m_clusters.End())
    {
        clusterIt = m_clusters.Insert(cell, InstanceGroupCluster {}).first;
    }

    clusterIt->second.instanceIds.PushBack(id);
    clusterIt->second.isDirty = true;
}

void InstanceGroup::RemoveFromCell(InstanceId id, Vec2i cell)
{
    if (m_needsFullRebuild)
    {
        return;
    }

    const auto clusterIt = m_clusters.Find(cell);

    if (clusterIt == m_clusters.End())
    {
        return;
    }

    Array<InstanceId>& instanceIds = clusterIt->second.instanceIds;

    const auto idIt = instanceIds.Find(id);

    if (idIt != instanceIds.End())
    {
        instanceIds.Erase(idIt);
    }

    clusterIt->second.isDirty = true;
}

void InstanceGroup::RebuildMembers()
{
    m_members.Clear();
    m_prefabBounds = BoundingBox::Empty();

    if (!m_prefab.IsValid())
    {
        return;
    }

    const Handle<Node>& root = m_prefab->GetRoot();

    if (!root.IsValid())
    {
        HYP_LOG(Scene, Warning, "InstanceGroup '{}': prefab '{}' has no root node", GetName(), m_prefab->GetName());

        return;
    }

    Array<Node*> nodes = root->GetDescendantsArray();
    nodes.PushFront(root.Get());

    uint32 numSkipped = 0;

    for (Node* node : nodes)
    {
        Entity* entity = DynamicCast<Entity>(node);

        const MeshComponent* meshComponent = entity != nullptr && entity->GetEntityManager() != nullptr
            ? entity->TryGetComponent<MeshComponent>()
            : nullptr;

        if (!meshComponent)
        {
            const Class* nodeClass = node->InstanceClass();

            // plain nodes and entities only give the meshes structure, anything else (lights, volumes, ...) isn't instanced
            if (nodeClass != Node::StaticClass() && nodeClass != Entity::StaticClass())
            {
                ++numSkipped;
            }

            continue;
        }

        if (!meshComponent->mesh.IsValid() || !meshComponent->material.IsValid() || meshComponent->skeleton.IsValid())
        {
            ++numSkipped;

            continue;
        }

        InstanceGroupMember member;
        member.mesh = meshComponent->mesh;
        member.material = meshComponent->material;
        member.matrix = Mat4f::identity;

        for (const Node* current = entity; current != nullptr && current != root.Get(); current = current->GetParent())
        {
            member.matrix = current->GetLocalTransform().GetMatrix() * member.matrix;
        }

        member.bounds = member.matrix * member.mesh->GetAABB();

        m_prefabBounds = m_prefabBounds.Union(member.bounds);

        m_members.PushBack(std::move(member));
    }

    if (numSkipped != 0)
    {
        HYP_LOG(Scene, Warning, "InstanceGroup '{}': {} node(s) of prefab '{}' aren't instanced (only static meshes are)",
            GetName(), numSkipped, m_prefab->GetName());
    }
}

void InstanceGroup::UpdateGroupBounds()
{
    BoundingBox groupBounds = BoundingBox::Empty();

    for (const auto& it : m_clusters)
    {
        for (const Handle<Entity>& entity : it.second.entities)
        {
            if (entity.IsValid() && entity->GetLocalBounds().IsValid())
            {
                groupBounds = groupBounds.Union(entity->GetLocalBounds());
            }
        }
    }

    SetLocalBounds(groupBounds.IsValid() ? groupBounds : BoundingBox::Zero());
}

bool InstanceGroup::UpdateClusters(Array<Handle<Entity>, SceneAllocator>& outReleasedEntities)
{
    AssertOnThread(g_simThread);

    if (m_needsFullRebuild)
    {
        ReleaseClusters(outReleasedEntities);

        RebuildMembers();
        EnsureInstancesLoaded();

        m_needsFullRebuild = false;

        for (InstanceEntry& entry : m_instances)
        {
            entry.cell = GetCell(entry.transform.GetTranslation());

            AddToCell(entry.id, entry.cell);
        }
    }

    if (m_instancesNeedStore)
    {
        StoreInstances();

        m_instancesNeedStore = false;
    }

    bool isMoving = false;

    Array<Vec2i, SceneTempAllocator> emptyCells;
    emptyCells.Reserve(m_clusters.Size());

    for (auto& it : m_clusters)
    {
        InstanceGroupCluster& cluster = it.second;

        if (!cluster.isDirty)
        {
            continue;
        }

        if (UpdateCluster(it.first, cluster, outReleasedEntities))
        {
            isMoving = true;
        }

        if (cluster.instanceIds.Empty())
        {
            emptyCells.PushBack(it.first);
        }
    }

    for (const Vec2i& cell : emptyCells)
    {
        m_clusters.Erase(cell);
    }

    UpdateGroupBounds();

    return isMoving;
}

bool InstanceGroup::UpdateCluster(Vec2i cell, InstanceGroupCluster& cluster, Array<Handle<Entity>, SceneAllocator>& outReleasedEntities)
{
    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        return false;
    }

    const uint32 numInstances = uint32(cluster.instanceIds.Size());

    if (numInstances == 0 || m_members.Empty())
    {
        for (Handle<Entity>& entity : cluster.entities)
        {
            if (entity.IsValid())
            {
                outReleasedEntities.PushBack(std::move(entity));
            }
        }

        cluster.entities.Clear();
        cluster.instanceData.Clear();
        cluster.isDirty = false;

        return false;
    }

    cluster.entities.Resize(m_members.Size());
    cluster.instanceData.Resize(m_members.Size());

    bool isMoving = false;

    Array<Mat4f> transforms;
    transforms.Resize(numInstances);

    Array<Mat4f> previousTransforms;
    previousTransforms.Resize(numInstances);

    for (uint32 memberIndex = 0; memberIndex < uint32(m_members.Size()); memberIndex++)
    {
        const InstanceGroupMember& member = m_members[memberIndex];

        BoundingBox bounds = BoundingBox::Empty();

        for (uint32 instanceIndex = 0; instanceIndex < numInstances; instanceIndex++)
        {
            const auto indexIt = m_instanceIndices.Find(uint32(cluster.instanceIds[instanceIndex]));
            AssertDebug(indexIt != m_instanceIndices.End());

            const Mat4f instanceMatrix = m_instances[indexIt->second].transform.GetMatrix();

            transforms[instanceIndex] = instanceMatrix * member.matrix;

            bounds = bounds.Union(instanceMatrix * member.bounds);
        }

        Handle<InstancedMeshData>& instanceData = cluster.instanceData[memberIndex];

        if (!instanceData.IsValid())
        {
            instanceData = MakeHandle<InstancedMeshData>(NAME_FMT("{}_{}_{}_{}", GetName(), cell.x, cell.y, memberIndex));
            instanceData->SetIsTransient(true);
            InitObject(instanceData);
        }

        {
            auto writeScope = instanceData->GetWriteScope();

            const BlobDataReference& currentBuffer = instanceData->buffers[0];

            const bool hasPreviousTransforms = currentBuffer.raw != nullptr
                && instanceData->bufferStructSizes[0] == sizeof(Mat4f)
                && currentBuffer.size == numInstances * sizeof(Mat4f);

            for (uint32 instanceIndex = 0; instanceIndex < numInstances; instanceIndex++)
            {
                previousTransforms[instanceIndex] = hasPreviousTransforms
                    ? static_cast<const Mat4f*>(currentBuffer.raw)[instanceIndex]
                    : transforms[instanceIndex];

                if (previousTransforms[instanceIndex] != transforms[instanceIndex])
                {
                    isMoving = true;
                }
            }

            instanceData->SetBufferData(0, transforms.Data(), numInstances);
            instanceData->SetBufferData(1, previousTransforms.Data(), numInstances);
        }

        Handle<Entity>& entity = cluster.entities[memberIndex];

        if (!entity.IsValid())
        {
            entity = MakeHandle<Entity>(NAME_FMT("{}_{}_{}_{}", GetName(), cell.x, cell.y, memberIndex));
            entity->SetIsStatic(IsStatic());

            // not part of the node hierarchy, so it's never saved, cloned or listed in the hierarchy, but it draws and culls like any entity of this scene
            entityManager->AddExistingEntity(entity);

            entity->AddComponent<InstanceClusterComponent>(InstanceClusterComponent {
                .group = WeakHandleFromThis(),
                .cell = cell,
                .memberIndex = memberIndex });

            MeshComponent meshComponent { member.mesh, member.material };
            meshComponent.instanceData = AssetReference(Handle<AssetObject>(instanceData));
            meshComponent.numInstances = numInstances;

            entity->AddComponent<MeshComponent>(std::move(meshComponent));
        }
        else
        {
            entity->GetComponent<MeshComponent>().numInstances = numInstances;
        }

        entity->SetLocalBounds(bounds);
        entity->SetNeedsRenderProxyUpdate();
    }

    // moving cells stay dirty for one more build, so the previous transforms catch up
    cluster.isDirty = isMoving;

    return isMoving;
}

void InstanceGroup::ReleaseClusters(Array<Handle<Entity>, SceneAllocator>& outReleasedEntities)
{
    for (auto& it : m_clusters)
    {
        for (Handle<Entity>& entity : it.second.entities)
        {
            if (entity.IsValid())
            {
                outReleasedEntities.PushBack(std::move(entity));
            }
        }
    }

    m_clusters.Clear();
}

HYP_NODISCARD InstanceId InstanceGroup::ReserveInstanceId()
{
    EnsureInstancesLoaded();

    const InstanceId id = InstanceId(m_nextId++);
    AssertDebug(id != InvalidInstanceId, "Out of instance ids");

    return id;
}

Handle<InstanceGroup> InstanceGroup::Find(Scene* scene, const Handle<Prefab>& prefab)
{
    AssertOnThread(g_simThread);

    if (!scene || !prefab.IsValid())
    {
        return Handle<InstanceGroup>::Null();
    }

    for (auto [existingGroup] : scene->GetEntityManager()->GetEntitySet<EntityType<InstanceGroup>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (existingGroup->GetPrefab() == prefab)
        {
            return MakeStrongRef(existingGroup);
        }
    }

    return Handle<InstanceGroup>::Null();
}

Handle<InstanceGroup> InstanceGroup::Create(Scene* scene, const Handle<Prefab>& prefab)
{
    AssertOnThread(g_simThread);

    if (!scene || !prefab.IsValid())
    {
        return Handle<InstanceGroup>::Null();
    }

    Handle<InstanceGroup> group = MakeHandle<InstanceGroup>(prefab);

    const String baseName = HYP_FORMAT("{}_Instances", prefab->GetName());
    group->SetName(scene->GetUniqueNodeName(baseName.Data()));

    return group;
}

} // namespace Hyperion
