/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Name/Name.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

class Node;

extern void Prefab_OnPostLoad(class Prefab&);

HYP_CLASS(AssetBucket = "Prefabs", PostLoad = "Prefab_OnPostLoad")
class ENGINE_API Prefab final : public AssetObject
{
    HYP_OBJECT_BODY(Prefab);

public:
    Prefab();
    explicit Prefab(Name name, const Handle<Node>& root = Handle<Node>::Null());

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    ~Prefab() override = default;

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    HYP_METHOD()
    const Handle<Node>& GetRoot() const;

    HYP_METHOD()
    void SetRoot(const Handle<Node>& root);

    /*! \brief Renames the root node to match this Prefab's name. Call after the asset registry may have
     *  given the Prefab a unique name (e.g. NewPrefab -> NewPrefab_6) without going through Rename() */
    void SyncRootName();

    HYP_METHOD()
    Handle<Node> Spawn() const;

    HYP_METHOD()
    static Handle<Prefab> Find(const ANSIStringView& nameStr);

    /*! \brief Look up a registered Prefab asset by its UUID */
    static Handle<Prefab> FindByUUID(const UUID& uuid);

    static UUID GetSourcePrefabUUID(const Node* node);

    static void TagAsPrefabInstance(Node* node, const UUID& prefabUUID);
    static void UntagAsPrefabInstance(Node* node);

private:
    HYP_FIELD(Property = "Root", Serialize)
    Handle<Node> m_root;
};

} // namespace Hyperion
