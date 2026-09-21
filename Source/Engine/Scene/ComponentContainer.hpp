/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/StridedBuffer.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/IndexAllocator.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/AnyRef.hpp>

#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Reflection/ObjId.hpp>
#include <Core/Util.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Entity;
class ComponentInterface;

enum class ComponentId : uint32;

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

/*! \brief Storage for every component of one type within an EntityManager.
 *  Components are stored as raw bytes with stable addresses; construction, copying, moving and destruction go through the
 *  type's ComponentInterface, so types registered at runtime are stored the same way as native ones. */
class ENGINE_API ComponentContainer
{
public:
    explicit ComponentContainer(const ComponentInterface& componentInterface);

    ComponentContainer(const ComponentContainer&) = delete;
    ComponentContainer& operator=(const ComponentContainer&) = delete;
    ComponentContainer(ComponentContainer&&) noexcept = delete;
    ComponentContainer& operator=(ComponentContainer&&) noexcept = delete;

    ~ComponentContainer();

    HYP_FORCE_INLINE const ComponentInterface& GetComponentInterface() const
    {
        return *m_componentInterface;
    }

    /*! \brief Gets the TypeInfo of the component type stored in this component container. */
    HYP_FORCE_INLINE const TypeInfo& GetComponentTypeInfo() const
    {
        return *m_typeInfo;
    }

    HYP_FORCE_INLINE TypeId GetComponentTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE uint32 GetNumComponents() const
    {
        return m_numComponents;
    }

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_numComponents == 0;
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

    HYP_FORCE_INLINE bool HasComponent(ComponentId id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.HasIndex(uint32(id));
    }

    HYP_FORCE_INLINE void* TryGetComponentRaw(ComponentId id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.GetElementRaw(uint32(id));
    }

    HYP_FORCE_INLINE const void* TryGetComponentRaw(ComponentId id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.GetElementRaw(uint32(id));
    }

    /*! \brief Tries to get the component with the given Id from the component container.
     *  \return A reference to the component if the component container has a component with the given Id, an empty reference otherwise. */
    HYP_FORCE_INLINE AnyRef TryGetComponent(ComponentId id)
    {
        void* component = TryGetComponentRaw(id);

        if (!component)
        {
            return AnyRef::Empty();
        }

        return AnyRef(m_typeInfo, component);
    }

    HYP_FORCE_INLINE ConstAnyRef TryGetComponent(ComponentId id) const
    {
        const void* component = TryGetComponentRaw(id);

        if (!component)
        {
            return ConstAnyRef::Empty();
        }

        return ConstAnyRef(m_typeInfo, component);
    }

    template <class Component>
    HYP_FORCE_INLINE Component& GetComponent(ComponentId id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        AssertDebug(m_typeId == TypeId::ForType<Component>(), "Component container holds TypeId {}, not `{}`", m_typeId.Value(), TypeNameWithoutNamespace<Component>().Data());

        void* component = m_components.GetElementRaw(uint32(id));

        AssertDebug(component != nullptr, "Component of type `{}` with ID {} does not exist", TypeNameWithoutNamespace<Component>().Data(), id);

        if (HYP_UNLIKELY(component == nullptr))
        {
            // Fall back to reference to static - since we return a reference
            // we need this and can't return null. But this should not happen!
            // Just needed to prevent destruction of the universe and everything within it
            static Component s_fallbackDefaultComponent {};
            return s_fallbackDefaultComponent;
        }

        return *static_cast<Component*>(component);
    }

    template <class Component>
    HYP_FORCE_INLINE const Component& GetComponent(ComponentId id) const
    {
        return const_cast<ComponentContainer*>(this)->GetComponent<Component>(id);
    }

    template <class Component>
    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(const Component& component)
    {
        AssertDebug(m_typeId == TypeId::ForType<Component>(), "Component container holds TypeId {}, not `{}`", m_typeId.Value(), TypeNameWithoutNamespace<Component>().Data());

        ComponentId id;
        void* memory = AllocateComponentSlot(id);

        Component* result = new (memory) Component(component);

        return Pair<ComponentId, Component&> { id, *result };
    }

    template <class Component, typename = std::enable_if_t<!std::is_reference_v<Component>>>
    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(Component&& component)
    {
        AssertDebug(m_typeId == TypeId::ForType<Component>(), "Component container holds TypeId {}, not `{}`", m_typeId.Value(), TypeNameWithoutNamespace<Component>().Data());

        ComponentId id;
        void* memory = AllocateComponentSlot(id);

        Component* result = new (memory) Component(std::move(component));

        return Pair<ComponentId, Component&> { id, *result };
    }

    /*! \brief Adds a default constructed component. */
    ComponentId AddDefaultComponent();

    /*! \brief Adds a component copy constructed from \p source, which must point to an instance of this container's component type. */
    ComponentId AddComponentCopy(const void* source);

    /*! \brief Adds a component move constructed from \p source, which must point to an instance of this container's component type. */
    ComponentId AddComponentMove(void* source);

    /*! \brief Adds a component copied from a BoxedValue holding (or referencing) this container's component type. */
    ComponentId AddComponent(const BoxedValue& componentData);

    /*! \brief Adds a component moved out of a BoxedValue holding (or referencing) this container's component type. */
    ComponentId AddComponent(BoxedValue&& componentData);

    /*! \brief Destroys the component with the given Id.
     *  \return True if the component was removed, false otherwise. */
    bool RemoveComponent(ComponentId id);

    /*! \brief Moves the component with the given Id into \p outBoxed, then removes it from the container.
     *  \return True if the component was removed, false otherwise. */
    bool RemoveComponent(ComponentId id, BoxedValue& outBoxed);

    /*! \brief Rebuilds every component for \p componentInterface */
    void MigrateTo(const ComponentInterface& componentInterface);

private:
    void* AllocateComponentSlot(ComponentId& outId);
    void FreeComponentSlot(ComponentId id);

    const ComponentInterface* m_componentInterface;
    const TypeInfo* m_typeInfo;
    TypeId m_typeId;

    // keeps a runtime-defined component Struct alive for as long as its components are
    ClassRef m_structReference;

    StridedBuffer<SceneAllocator> m_components;
    IndexAllocator m_componentIdAllocator;
    uint32 m_numComponents;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
