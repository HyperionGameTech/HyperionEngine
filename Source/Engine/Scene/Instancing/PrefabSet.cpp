/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Instancing/PrefabSet.hpp>

#include <Scene/Prefab.hpp>

#include <PrefabSet.generated.inl>

namespace Hyperion {

PrefabSet::PrefabSet()
    : PrefabSet(Name::Invalid())
{
}

PrefabSet::PrefabSet(Name name)
    : AssetObject(name)
{
}

void PrefabSet::AddEntry(const Handle<Prefab>& prefab, float weight)
{
    if (!prefab.IsValid())
    {
        return;
    }

    PrefabSetEntry entry;
    entry.prefab = prefab;
    entry.weight = weight;

    m_entries.PushBack(std::move(entry));
}

Array<PrefabSetEntry> PrefabSet::GetValidEntries() const
{
    Array<PrefabSetEntry> valid;

    for (const PrefabSetEntry& entry : m_entries)
    {
        if (entry.prefab.IsValid() && entry.weight > 0.0f)
        {
            valid.PushBack(entry);
        }
    }

    return valid;
}

Handle<Prefab> PrefabSet::ChooseOne(float unit) const
{
    float totalWeight = 0.0f;

    for (const PrefabSetEntry& entry : m_entries)
    {
        if (entry.prefab.IsValid() && entry.weight > 0.0f)
        {
            totalWeight += entry.weight;
        }
    }

    if (totalWeight <= 0.0f)
    {
        return Handle<Prefab>::Null();
    }

    float remaining = unit * totalWeight;
    const PrefabSetEntry* last = nullptr;

    for (const PrefabSetEntry& entry : m_entries)
    {
        if (!entry.prefab.IsValid() || entry.weight <= 0.0f)
        {
            continue;
        }

        last = &entry;

        if (remaining < entry.weight)
        {
            return entry.prefab;
        }

        remaining -= entry.weight;
    }

    return last->prefab;
}

} // namespace Hyperion
