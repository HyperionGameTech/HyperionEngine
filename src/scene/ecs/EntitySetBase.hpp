#ifndef HYPERION_V2_ECS_ENTITY_SET_BASE_HPP
#define HYPERION_V2_ECS_ENTITY_SET_BASE_HPP

#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/IDCreator.hpp>
#include <core/ID.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <Types.hpp>

#include <atomic>

namespace hyperion::v2 {

class Entity;

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

    EntitySetBase()                                             = default;
    EntitySetBase(const EntitySetBase &other)                   = delete;
    EntitySetBase &operator=(const EntitySetBase &other)        = delete;
    EntitySetBase(EntitySetBase &&other) noexcept               = delete;
    EntitySetBase &operator=(EntitySetBase &&other) noexcept    = delete;
    virtual ~EntitySetBase()                                    = default;

    /*! \brief Checks if an Entity's components are valid for this EntitySet.
     *
     *  \param entity The Entity to check.
     *
     *  \return True if the Entity's components are valid for this EntitySet, false otherwise.
     */
    virtual Bool ValidForEntity(ID<Entity> entity) const = 0;

    /*! \brief To be used by the EntityManager */
    virtual void OnEntityUpdated(ID<Entity> entity) = 0;
};

} // namespace hyperion::v2

#endif