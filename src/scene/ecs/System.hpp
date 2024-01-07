#ifndef HYPERION_V2_ECS_SYSTEM_HPP
#define HYPERION_V2_ECS_SYSTEM_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeID.hpp>
#include <scene/ecs/EntitySet.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <GameCounter.hpp>

#include <tuple>

namespace hyperion::v2 {

class EntityManager;

class SystemBase
{
public:
    virtual ~SystemBase() = default;

    /*! \brief Returns the TypeIDs of the components this System operates on.
     *  To be used by the EntityManager in order to properly order the Systems based on their dependencies.
     *
     *  \return The TypeIDs of the components this System operates on.
     */
    const Array<TypeID> &GetComponentTypeIDs() const
        { return m_component_type_ids; }


    /*! \brief Returns true if this System operates on the component with the given TypeID, false otherwise.
     *
     *  \param component_type_id The TypeID of the component to check.
     *  \param include_read_only If true, this function will return true even if the component is read-only.
     *  Otherwise, read-only components will be ignored.
     *
     *  \return True if this System operates on the component with the given TypeID, false otherwise.
     */
    Bool HasComponentTypeID(TypeID component_type_id, Bool include_read_only = true) const
    {
        const Bool has_component_type_id = m_component_type_ids.Contains(component_type_id);

        if (!has_component_type_id) {
            return false;
        }

        if (include_read_only) {
            return true;
        }

        const ComponentRWFlags rw_flags = GetComponentRWFlags(component_type_id);

        return rw_flags != COMPONENT_RW_FLAGS_READ;
    }

    /*! \brief Returns the ComponentRWFlags of the component with the given TypeID.
     *
     *  \param component_type_id The TypeID of the component to check.
     *
     *  \return The ComponentRWFlags of the component with the given TypeID.
     */
    ComponentRWFlags GetComponentRWFlags(TypeID component_type_id) const
    {
        const auto it = m_component_type_ids.Find(component_type_id);
        AssertThrowMsg(it != m_component_type_ids.End(), "Component type ID not found");

        const SizeType index = m_component_type_ids.IndexOf(it);
        AssertThrowMsg(index != SizeType(-1), "Component type ID not found");

        return m_component_rw_flags[index];
    }

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) = 0;

protected:
    SystemBase(Array<TypeID> &&component_type_ids, Array<ComponentRWFlags> &&component_rw_flags)
        : m_component_type_ids(std::move(component_type_ids)),
          m_component_rw_flags(std::move(component_rw_flags))
    {
        AssertThrowMsg(m_component_type_ids.Size() == m_component_rw_flags.Size(), "Component type ID count and component RW flags count mismatch");
    }

    Array<TypeID>           m_component_type_ids;
    Array<ComponentRWFlags> m_component_rw_flags;
};

/*! \brief A System is a class that operates on a set of components.
 *
 *  \tparam ComponentDescriptors ComponentDescriptor structs for the Components this System operates on.
 */
template <class ... ComponentDescriptors>
class System : public SystemBase
{
public:
    using ComponentDescriptorTypes = std::tuple<ComponentDescriptors...>;

    System()
        : SystemBase({ TypeID::ForType<typename ComponentDescriptors::Type>()... }, { ComponentDescriptors::rw_flags... })
    {
    }

    System(const System &other)                 = delete;
    System &operator=(const System &other)      = delete;
    System(System &&other) noexcept             = delete;
    System &operator=(System &&other) noexcept  = delete;
    virtual ~System() override                  = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override = 0;
};

} // namespace hyperion::v2

#endif