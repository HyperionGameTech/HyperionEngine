/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Components/SwatchOverridesComponent.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <SwatchOverridesComponent.generated.inl>

namespace Hyperion {

static bool HandleSwatchOverridesSection(BoxedValue& owner, Array<HMF::SchemaSectionEntry>&& entries)
{
    Entity* entity = nullptr;

    if (!owner.IsNull())
    {
        void* pointer = owner.ToRef().GetPointer();

        if (pointer != nullptr)
        {
            auto* object = reinterpret_cast<ObjectBase*>(pointer);

            if (IsA(Entity::StaticClass(), object->InstanceClass()))
            {
                entity = Cast<Entity>(object);
            }
        }
    }

    if (!entity)
    {
        return false;
    }

    Array<EntitySwatchOverrideSet> sets;
    sets.Reserve(entries.Size());

    for (HMF::SchemaSectionEntry& entry : entries)
    {
        EntitySwatchOverrideSet set;
        set.swatchName = entry.key;
        set.propertyOverrides.Reserve(entry.values.Size());

        for (Pair<Name, BoxedValue>& value : entry.values)
        {
            SwatchPropertyOverride overrideEntry;
            overrideEntry.property = value.first;
            overrideEntry.value = std::move(value.second);

            set.propertyOverrides.PushBack(std::move(overrideEntry));
        }

        sets.PushBack(std::move(set));
    }

    entity->SetPendingSwatchOverrides(std::move(sets));
    entity->FlushPendingSwatchOverrides();

    return true;
}

///DI for $Swatches parsing.

static struct InitializeSwatchOverridesSinks
{
    InitializeSwatchOverridesSinks()
    {
        HMF::SetParseSchemaSectionFn("Swatches", &HandleSwatchOverridesSection);
    }
} s_installSwatchOverridesSink;

} // namespace Hyperion
