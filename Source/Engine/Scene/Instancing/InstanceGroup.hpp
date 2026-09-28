/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Mat4f.hpp>

#include <Scene/Entity.hpp>

#include <Scene/Instancing/InstanceTypes.hpp>

namespace Hyperion {

class Prefab;
class Mesh;
class Material;
class Scene;
class InstanceSetData;
class InstancedMeshData;
class InstanceGroupSystem;

struct InstanceGroupMember
{
    Handle<Mesh> mesh;
    Handle<Material> material;

    Mat4f matrix;

    /// bounds of the mesh placed by \ref matrix, in prefab root space
    BoundingBox bounds;
};

struct InstanceGroupCluster
{
    Array<InstanceId> instanceIds;

    Array<Handle<Entity>> entities;
    Array<Handle<InstancedMeshData>> instanceData;

    bool isDirty = true;
};

HYP_CLASS()
class ENGINE_API InstanceGroup final : public Entity
{
    HYP_OBJECT_BODY(InstanceGroup);

    friend class InstanceGroupSystem;

public:
    InstanceGroup();
    explicit InstanceGroup(const Handle<Prefab>& prefab);

    InstanceGroup(const InstanceGroup&) = delete;
    InstanceGroup& operator=(const InstanceGroup&) = delete;

    ~InstanceGroup() override;

    virtual Handle<Node> Clone() const override;

    /// the static meshes of \p prefab, placed in prefab root space.
    /// Returns how many of its nodes can't be instanced.
    static uint32 CollectMembers(const Prefab& prefab, Array<InstanceGroupMember>& outMembers);

    HYP_METHOD(Property = "Prefab", Serialize, Editor)
    HYP_FORCE_INLINE const Handle<Prefab>& GetPrefab() const
    {
        return m_prefab;
    }

    HYP_METHOD(Property = "Prefab", Serialize, Editor)
    void SetPrefab(const Handle<Prefab>& prefab);

    HYP_METHOD(Property = "InstanceData", Serialize, Editor = false)
    HYP_FORCE_INLINE const Handle<InstanceSetData>& GetInstanceData() const
    {
        return m_instanceData;
    }

    HYP_METHOD(Property = "InstanceData", Serialize, Editor = false)
    void SetInstanceData(const Handle<InstanceSetData>& instanceData);

    HYP_METHOD(Property = "ClusterSize", Serialize, Editor)
    HYP_FORCE_INLINE float GetClusterSize() const
    {
        return m_clusterSize;
    }

    HYP_METHOD(Property = "ClusterSize", Serialize, Editor)
    void SetClusterSize(float clusterSize);

    HYP_METHOD(Property = "InstanceCount", Editor, EditEnabled = false, Transient)
    HYP_FORCE_INLINE uint32 NumInstances() const
    {
        return uint32(m_instances.Size());
    }

    InstanceId AddInstance(const Transform& transform);

    /// Adds the instance keeping its id. Does nothing and returns false if the id is already in use.
    bool AddInstanceWithId(InstanceId id, const Transform& transform);

    bool RemoveInstance(InstanceId id);

    bool SetInstanceTransform(InstanceId id, const Transform& transform);

    bool GetInstanceTransform(InstanceId id, Transform& outTransform) const;

    /// The instance drawn at \p instanceIndex by the cluster entity for \p cell
    InstanceId GetClusterInstanceId(Vec2i cell, uint32 instanceIndex) const;

    HYP_FORCE_INLINE const BoundingBox& GetPrefabBounds() const
    {
        return m_prefabBounds;
    }

    HYP_NODISCARD InstanceId ReserveInstanceId();

    static Handle<InstanceGroup> Find(Scene* scene, const Handle<Prefab>& prefab);
    static Handle<InstanceGroup> Create(Scene* scene, const Handle<Prefab>& prefab);

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Rebuild Instances")
    void RebuildInstances();
#endif

protected:
    void Init() override;

private:
    struct InstanceEntry
    {
        InstanceId id;
        Transform transform;
        Vec2i cell;
    };

    Vec2i GetCell(const Vec3f& position) const;

    void EnsureInstancesLoaded();
    void StoreInstances();

    void OnInstancesChanged();

    void MarkCellDirty(Vec2i cell);
    void MarkAllCellsDirty();

    void AddToCell(InstanceId id, Vec2i cell);
    void RemoveFromCell(InstanceId id, Vec2i cell);

    void RebuildMembers();
    void UpdateGroupBounds();

    /// Rebuilds dirty cells.
    /// Returns true if an instance changed since the last build
    bool UpdateClusters(Array<Handle<Entity>, SceneAllocator>& outReleasedEntities);

    bool UpdateCluster(
        Vec2i cell,
        InstanceGroupCluster& cluster,
        Array<Handle<Entity>, SceneAllocator>& outReleasedEntities);

    void ReleaseClusters(Array<Handle<Entity>, SceneAllocator>& outReleasedEntities);

    Handle<Prefab> m_prefab;
    Handle<InstanceSetData> m_instanceData;

    float m_clusterSize;

    // runtime state, rebuilt from m_instanceData and m_prefab

    Array<InstanceEntry> m_instances;
    Map<uint32, uint32> m_instanceIndices;
    uint32 m_nextId;

    Array<InstanceGroupMember> m_members;
    BoundingBox m_prefabBounds;

    Map<Vec2i, InstanceGroupCluster> m_clusters;

    uint32 m_revision;

    bool m_instancesLoaded : 1;
    bool m_instancesNeedStore : 1;
    bool m_needsFullRebuild : 1;
};

} // namespace Hyperion
