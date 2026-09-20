/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Name/Name.hpp>

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
    const Handle<Node>& GetRoot() const;

    HYP_METHOD()
    void SetRoot(const Handle<Node>& root);

    HYP_METHOD()
    Handle<Node> Spawn() const;

    static UUID GetSourcePrefabUUID(const Node* node);

private:
    HYP_FIELD(Property = "Root", Serialize)
    Handle<Node> m_root;
};

} // namespace Hyperion
