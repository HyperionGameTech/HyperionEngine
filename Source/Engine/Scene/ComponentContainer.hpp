/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/AnyRef.hpp>
#include <Core/Memory/MemoryPool.hpp>

#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Reflection/ObjId.hpp>
#include <Core/Util.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Entity;

enum class ComponentId : uint32;
static constexpr ComponentId InvalidComponentId = ComponentId(0);

HYP_ENUM()
enum class ComponentAccess : uint8
{
    NONE = 0,
    READ = 0x1,
    WRITE = 0x2,
    READ_WRITE = READ | WRITE
};

HYP_MAKE_ENUM_FLAGS(ComponentAccess);

template <class T, EnumFlags<ComponentAccess> TAccess = ComponentAccess::READ_WRITE, bool TReceivesEvents = true>
struct ComponentDescriptor
{
    using Type = T;

    constexpr static EnumFlags<ComponentAccess> Access = TAccess;
    constexpr static bool ReceivesEvents = TReceivesEvents;
};

HYP_STRUCT(Size = 8)
struct ComponentInfo
{
    HYP_STRUCT_BODY(ComponentInfo);

    HYP_FIELD()
    TypeId typeId;

    HYP_FIELD()
    EnumFlags<ComponentAccess> access;

    HYP_FIELD()
    bool receivesEvents;

    ComponentInfo()
        : typeId(TypeId::Void()),
          access(ComponentAccess::NONE),
          receivesEvents(false)
    {
    }

    ComponentInfo(TypeId typeId, EnumFlags<ComponentAccess> access = ComponentAccess::NONE, bool receivesEvents = false)
        : typeId(typeId),
          access(access),
          receivesEvents(receivesEvents)
    {
    }

    template <class ComponentDescriptorType>
    ComponentInfo(ComponentDescriptorType)
        : typeId(TypeId::ForType<typename NormalizedType<ComponentDescriptorType>::Type>()),
          access(NormalizedType<ComponentDescriptorType>::Access),
          receivesEvents(NormalizedType<ComponentDescriptorType>::ReceivesEvents)
    {
    }
};

class ComponentContainerFactoryBase;

class ENGINE_API ComponentContainerBase
{
public:
    ComponentContainerBase(ComponentContainerFactoryBase* factory)
        : m_factory(factory)
    {
    }

    ComponentContainerBase(const ComponentContainerBase&) = delete;
    ComponentContainerBase& operator=(const ComponentContainerBase&) = delete;
    ComponentContainerBase(ComponentContainerBase&&) noexcept = delete;
    ComponentContainerBase& operator=(ComponentContainerBase&&) noexcept = delete;

    virtual ~ComponentContainerBase() = default;

    HYP_FORCE_INLINE ComponentContainerFactoryBase* GetFactory() const
    {
        return m_factory;
    }

#ifdef HYP_ENABLE_MT_CHECK
    HYP_FORCE_INLINE DataRaceDetector& GetDataRaceDetector()
    {
        return m_dataRaceDetector;
    }

    HYP_FORCE_INLINE const DataRaceDetector& GetDataRaceDetector() const
    {
        return m_dataRaceDetector;
    }
#endif

    /*! \brief Gets the TypeInfo of the component type stored in this component container. */
    virtual const TypeInfo& GetComponentTypeInfo() const = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given Id, nullptr otherwise.
     */
    virtual AnyRef TryGetComponent(ComponentId id) = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given Id, nullptr otherwise.
     */
    virtual ConstAnyRef TryGetComponent(ComponentId id) const = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *  \param outBoxed The value to store a reference to the component in
     *
     *  \return True if the component was found, false otherwise
     */
    bool TryGetComponent(ComponentId id, BoxedValue& outBoxed);

    /*! \brief Checks if the component container has a component with the given Id.
     *
     *  \param id The Id of the component to check.
     *
     *  \return True if the component container has a component with the given Id, false otherwise.
     */
    virtual bool HasComponent(ComponentId id) const = 0;

    /*! \brief Adds a component to the component container, using BoxedValue to store the component data generically.
     *
     *  \param componentData The BoxedValue containing the component data to add.
     *
     *  \return The Id of the added component.
     */
    virtual ComponentId AddComponent(const BoxedValue& componentData) = 0;

    /*! \brief Adds a component to the component container, using BoxedValue to store the component data generically.
     *
     *  \param componentData The BoxedValue containing the component data to add.
     *
     *  \return The Id of the added component.
     */
    virtual ComponentId AddComponent(BoxedValue&& componentData) = 0;

    /*! \brief Removes the component with the given Id from the component container.
     *
     *  \param id The Id of the component to remove.
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual bool RemoveComponent(ComponentId id) = 0;

    /*! \brief Removes the component with the given Id from the component container and stores the component object in BoxedValue
     *
     *  \param id The Id of the component to remove.
     *  \param outBoxed Out reference to store the component data in
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual bool RemoveComponent(ComponentId id, BoxedValue& outBoxed) = 0;

    /*! \brief Moves the component with the given Id from this component container to the given component container.
     *       The component container must be of the same type as this component container, otherwise an assertion will be thrown.
     *
     *  \param id The Id of the component to move.
     *  \param other The component container to move the component to.
     *
     *  \return An optional containing the Id of the component in the given component container if the component was moved, an empty optional otherwise.
     */
    virtual Optional<ComponentId> MoveComponent(ComponentId id, ComponentContainerBase& other) = 0;

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);

private:
    ComponentContainerFactoryBase* m_factory;
};

class ComponentContainerFactoryBase
{
protected:
    ComponentContainerFactoryBase()
        : m_createFn(nullptr)
    {
    }

public:
    HYP_FORCE_INLINE UniquePtr<ComponentContainerBase, SceneAllocator> Create() const
    {
        return m_createFn();
    }

protected:
    UniquePtr<ComponentContainerBase, SceneAllocator> (*m_createFn)(void);
};

template <class Component>
class ComponentContainer final : public ComponentContainerBase
{
    static class FactoryInstance final : public ComponentContainerFactoryBase
    {
    public:
        FactoryInstance(UniquePtr<ComponentContainerBase, SceneAllocator> (*createFn)(void))
        {
            m_createFn = createFn;
        }
    } s_factoryInstance;

public:
    static ComponentContainerFactoryBase* GetFactory()
    {
        return &s_factoryInstance;
    }

    ComponentContainer()
        : ComponentContainerBase(&s_factoryInstance)
    {
    }

    ComponentContainer(const ComponentContainer&) = delete;
    ComponentContainer& operator=(const ComponentContainer&) = delete;

    ComponentContainer(ComponentContainer&&) noexcept = delete;
    ComponentContainer& operator=(ComponentContainer&&) noexcept = delete;

    virtual ~ComponentContainer() override = default;

    virtual const TypeInfo& GetComponentTypeInfo() const override
    {
        return TypeOf<Component>();
    }

    virtual bool HasComponent(ComponentId id) const override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.Contains(id);
    }

    virtual AnyRef TryGetComponent(ComponentId id) override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it == m_components.End())
        {
            return AnyRef::Empty();
        }

        return AnyRef(&it->second);
    }

    virtual ConstAnyRef TryGetComponent(ComponentId id) const override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it == m_components.End())
        {
            return ConstAnyRef::Empty();
        }

        return ConstAnyRef(&it->second);
    }

    HYP_FORCE_INLINE Component& GetComponent(ComponentId id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        Assert(HasComponent(id), "Component of type `{}` with ID {} does not exist", TypeNameWithoutNamespace<Component>().Data(), id);

        return m_components.At(id);
    }

    HYP_FORCE_INLINE const Component& GetComponent(ComponentId id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        Assert(HasComponent(id), "Component of type `{}` with ID {} does not exist", TypeNameWithoutNamespace<Component>().Data(), id);

        return m_components.At(id);
    }

    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(const Component& component)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        ComponentId id = ComponentId(++m_componentIdCounter);

        auto insertResult = m_components.Set(id, component);

        return Pair<ComponentId, Component&> { id, insertResult.first->second };
    }

    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(Component&& component)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        ComponentId id = ComponentId(++m_componentIdCounter);

        auto insertResult = m_components.Set(id, std::move(component));

        return Pair<ComponentId, Component&> { id, insertResult.first->second };
    }

    virtual ComponentId AddComponent(const BoxedValue& componentData) override
    {
        Assert(componentData.IsValid(), "Cannot add an invalid component");
        Assert(componentData.Is<Component>(), "Component data is not of the correct type");

        return AddComponent(componentData.Get<Component>()).first;
    }

    virtual ComponentId AddComponent(BoxedValue&& componentData) override
    {
        Assert(componentData.IsValid(), "Cannot add an invalid component");
        Assert(componentData.Is<Component>(), "Component is not of the correct type");

        return AddComponent(std::move(componentData.Get<Component>())).first;
    }

    virtual bool RemoveComponent(ComponentId id) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            m_components.Erase(it);

            return true;
        }

        return false;
    }

    virtual bool RemoveComponent(ComponentId id, BoxedValue& outBoxed) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            outBoxed = BoxedValue(std::move(it->second));

            m_components.Erase(it);

            return true;
        }

        return false;
    }

    virtual Optional<ComponentId> MoveComponent(ComponentId id, ComponentContainerBase& other) override
    {
        AssertDebug(other.GetComponentTypeInfo() == GetComponentTypeInfo(), "Component container is not of the same type");

        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            Component& component = it->second;

            const ComponentId newComponentId = static_cast<ComponentContainer<Component>&>(other).AddComponent(std::move(component)).first;

            m_components.Erase(it);

            return newComponentId;
        }

        return {};
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.Size();
    }

private:
    uint32 m_componentIdCounter = 0;

    TMap<ComponentId, Component, SceneAllocator> m_components;
};

template <class Component>
typename ComponentContainer<Component>::FactoryInstance ComponentContainer<Component>::s_factoryInstance {
    []() -> UniquePtr<ComponentContainerBase, SceneAllocator>
    {
        return MakeUniqueWithAllocator<ComponentContainer<Component>, SceneAllocator>();
    }
};

} // namespace Hyperion
