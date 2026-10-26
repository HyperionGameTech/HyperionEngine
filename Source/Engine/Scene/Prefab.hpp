/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetImportInfo.hpp>

#include <Core/Name/Name.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

class Node;
class Entity;
class World;
class Mesh;
class Skeleton;

extern void Prefab_OnPostLoad(class Prefab&);

HYP_CLASS(AssetBucket = "Prefabs", PostLoad = "Prefab_OnPostLoad")
class ENGINE_API Prefab final : public AssetObject
{
    HYP_OBJECT_BODY(Prefab);

    friend void Prefab_OnPostLoad(Prefab&);

public:
    Prefab();
    explicit Prefab(Name name, const Handle<Node>& root = Handle<Node>::Null());

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    ~Prefab() override = default;

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    HYP_METHOD()
    Handle<Entity> GetRoot() const;

    HYP_METHOD()
    void SetRoot(const Handle<Node>& root);

    HYP_METHOD()
    uint32 GetRevision() const;

    void SetRevision(uint32 revision);

    ///Renames the root node to match this Prefab's name 
    void SyncRootName();

    HYP_METHOD()
    Handle<Node> Spawn() const;

    Handle<Node> SpawnReplacementFor(const Node* instance) const;

    /*! \brief Finds the mesh with the largest world-space bounds in this Prefab's node hierarchy.
     *  Used in places where we allow importing a model from a Mesh selector.
     *  \param outMesh Set to the largest mesh, or null if there are none
     *  \returns The number of distinct meshes in the Prefab */
    uint32 FindLargestMesh(Handle<Mesh>& outMesh) const;

    HYP_METHOD()
    Handle<Mesh> GetLargestMesh() const;

    HYP_METHOD()
    uint32 CountMeshes() const;

    HYP_METHOD()
    static Handle<Prefab> Find(const ANSIStringView& nameStr);

    ///Look up a registered Prefab asset by its UUID
    static Handle<Prefab> FindByUUID(const UUID& uuid);

    ///Every live instance of this Prefab across the world's foreground scenes
    Array<Handle<Node>> FindLiveInstances(const World* world) const;

    static UUID GetSourcePrefabUUID(const Node* node);

    static uint32 GetInstanceRevision(const Node* node);

    static void TagAsPrefabInstance(Node* node, const UUID& prefabUUID, uint32 revision = 0);
    static void UntagAsPrefabInstance(Node* node);

    static uint32 SyncStaleInstances(Node* root);

    const AssetImportInfo& GetImportInfo() const
    {
        return m_importInfo;
    }

    void SetImportInfo(const AssetImportInfo& importInfo)
    {
        m_importInfo = importInfo;
    }

    // the rig a model loader built, reachable even when no skinned mesh uses it; not saved
    const Handle<Skeleton>& GetLoadedSkeleton() const
    {
        return m_loadedSkeleton;
    }

    void SetLoadedSkeleton(const Handle<Skeleton>& skeleton)
    {
        m_loadedSkeleton = skeleton;
    }

    static Delegate<void, Prefab*> OnPrefabChanged;

private:
    HYP_FIELD(Property = "Root", Serialize)
    Handle<Node> m_root;

    HYP_FIELD(Property = "Revision", Serialize)
    uint32 m_revision = 0;

    HYP_FIELD(Property = "ImportInfo", Serialize, Editor = false)
    AssetImportInfo m_importInfo;

    Handle<Skeleton> m_loadedSkeleton;
};

} // namespace Hyperion
