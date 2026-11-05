/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Prefab;

HYP_STRUCT()
struct PrefabSetEntry
{
    HYP_STRUCT_BODY(PrefabSetEntry);

    HYP_FIELD(Property = "Prefab", Serialize, Editor)
    Handle<Prefab> prefab;

    HYP_FIELD(Property = "Weight", Serialize, Editor)
    float weight = 1.0f;
};

HYP_CLASS(AssetBucket = "Misc")
class ENGINE_API PrefabSet final : public AssetObject
{
    HYP_OBJECT_BODY(PrefabSet);

public:
    PrefabSet();
    explicit PrefabSet(Name name);

    virtual ~PrefabSet() override = default;

    const Array<PrefabSetEntry>& GetEntries() const
    {
        return m_entries;
    }

    void SetEntries(const Array<PrefabSetEntry>& entries)
    {
        m_entries = entries;
    }

    HYP_METHOD()
    void AddEntry(const Handle<Prefab>& prefab, float weight = 1.0f);

    Array<PrefabSetEntry> GetValidEntries() const;

    HYP_METHOD()
    Handle<Prefab> ChooseOne(float unit) const;

private:
    HYP_FIELD(Property = "Entries", Serialize, Editor)
    Array<PrefabSetEntry> m_entries;
};

} // namespace Hyperion
