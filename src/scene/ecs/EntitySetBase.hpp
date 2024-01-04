#ifndef HYPERION_V2_ECS_ENTITY_SET_BASE_HPP
#define HYPERION_V2_ECS_ENTITY_SET_BASE_HPP

#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/IDCreator.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <Types.hpp>

#include <atomic>

namespace hyperion::v2 {

using EntitySetTypeID = UInt;

class EntitySetBase;
class ComponentContainerBase;

struct EntitySetIDGeneratorBase
{
    static inline std::atomic<UInt> counter { 0u };
};

template <class ... Components>
struct EntitySetIDGenerator : EntitySetIDGeneratorBase
{
    static EntitySetTypeID GetID()
    {
        static EntitySetTypeID id = ++EntitySetIDGeneratorBase::counter;

        return id;
    }
};

class EntitySetBase
{
public:
    template <class ... Components>
    static EntitySetTypeID GenerateEntitySetTypeID()
    {
        return EntitySetIDGenerator<Components...>::GetID();
    }
};

} // namespace hyperion::v2

#endif