/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ClassAttribute.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Scene/EntityTag.hpp>

#include <type_traits>

namespace Hyperion {

class ComponentInterfaceRegistry;
class Struct;
class ObjectBase;

template <EntityTag Tag>
struct TagComponent;

enum class ComponentInterfaceFlags : uint32
{
    NONE = 0x0,
    ENTITY_TAG = 0x1,
    SERIALIZE = 0x2,
    REPLICATED = 0x4,
    SHOW_IN_EDITOR = 0x8,
    RUNTIME = 0x10
};

HYP_MAKE_ENUM_FLAGS(ComponentInterfaceFlags)

namespace Attributes {
CORE_API extern const Name g_attrSerialize;
CORE_API extern const Name g_attrReplicated;
} // namespace Attributes

class ENGINE_API ComponentInterface
{
public:
    ComponentInterface(const TypeInfo* typeInfo, const Struct* componentStruct, EnumFlags<ComponentInterfaceFlags> flags, EntityTag entityTag = EntityTag(~0ull));

    ComponentInterface(const ComponentInterface&) = delete;
    ComponentInterface& operator=(const ComponentInterface&) = delete;
    ComponentInterface(ComponentInterface&&) noexcept = delete;
    ComponentInterface& operator=(ComponentInterface&&) noexcept = delete;

    ~ComponentInterface();

    static UniquePtr<ComponentInterface> CreateForStruct(const TypeInfo* typeInfo, size_t expectedSize, size_t expectedAlignment, bool shouldSerialize);
    static UniquePtr<ComponentInterface> CreateForEntityTag(const TypeInfo* typeInfo, size_t expectedSize, size_t expectedAlignment, EntityTag entityTag, bool shouldSerialize, bool showInEditor);

    HYP_FORCE_INLINE const TypeInfo& GetTypeInfo() const
    {
        return *m_typeInfo;
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    const Class* GetClass() const;

    HYP_FORCE_INLINE const Struct* GetStruct() const
    {
        return m_struct;
    }

    size_t GetComponentSize() const;
    size_t GetComponentAlignment() const;

    HYP_FORCE_INLINE EnumFlags<ComponentInterfaceFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE bool GetShouldSerialize() const
    {
        return m_flags & ComponentInterfaceFlags::SERIALIZE;
    }

    HYP_FORCE_INLINE bool ShouldShowInEditor() const
    {
        return m_flags & ComponentInterfaceFlags::SHOW_IN_EDITOR;
    }

    HYP_FORCE_INLINE bool IsEntityTag() const
    {
        return m_flags & ComponentInterfaceFlags::ENTITY_TAG;
    }

    HYP_FORCE_INLINE EntityTag GetEntityTag() const
    {
        return m_entityTag;
    }

    HYP_FORCE_INLINE bool IsReplicated() const
    {
        return m_flags & ComponentInterfaceFlags::REPLICATED;
    }

    HYP_FORCE_INLINE bool IsRuntimeComponent() const
    {
        return m_flags & ComponentInterfaceFlags::RUNTIME;
    }

    void ConstructComponent(void* destination) const;
    void CopyConstructComponent(void* destination, const void* source) const;
    void MoveConstructComponent(void* destination, void* source) const;
    void DestructComponent(void* target) const;

    bool CreateInstance(BoxedValue& out) const;

private:
    friend class ComponentInterfaceRegistry;

    void ReleaseStructReference();

    const TypeInfo* m_typeInfo;
    TypeId m_typeId;
    const Struct* m_struct;
    ClassRef m_structReference;
    EntityTag m_entityTag;
    EnumFlags<ComponentInterfaceFlags> m_flags;
};

class ENGINE_API ComponentInterfaceRegistry
{
public:
    static ComponentInterfaceRegistry& GetInstance();

    ComponentInterfaceRegistry();
    ~ComponentInterfaceRegistry();

    ComponentInterfaceRegistry(const ComponentInterfaceRegistry&) = delete;
    ComponentInterfaceRegistry& operator=(const ComponentInterfaceRegistry&) = delete;

    void Initialize();
    void Shutdown();

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_isInitialized;
    }

    HYP_FORCE_INLINE uint32 GetRuntimeRegistrationGeneration() const
    {
        return m_runtimeRegistrationGeneration.Get(MemoryOrder::ACQUIRE);
    }

    void Register(TypeId typeId, UniquePtr<ComponentInterface> (*createFunction)());

    const ComponentInterface* RegisterRuntimeComponent(
        const Struct* componentStruct,
        EnumFlags<ComponentInterfaceFlags> flags = ComponentInterfaceFlags::SERIALIZE | ComponentInterfaceFlags::SHOW_IN_EDITOR);

    bool UnregisterRuntimeComponent(TypeId typeId);

    const ComponentInterface* GetComponentInterface(TypeId typeId) const;
    const ComponentInterface* GetEntityTagComponentInterface(EntityTag tag) const;
    Array<const ComponentInterface*> GetComponentInterfaces() const;

private:
    struct LookupTable
    {
        Map<TypeId, const ComponentInterface*> interfacesByTypeId;
        Map<uint64, const ComponentInterface*> interfacesByEntityTag;
    };

    const LookupTable& GetLookupTable() const;

    UniquePtr<LookupTable> CopyLookupTable() const;
    void PublishLookupTable(UniquePtr<LookupTable>&& lookupTable);
    static void AddToLookupTable(LookupTable& lookupTable, const ComponentInterface* componentInterface);

    Map<TypeId, UniquePtr<ComponentInterface> (*)()> m_nativeCreateFunctions;

    // Interfaces are never destroyed before the registry itself, so containers referencing them never dangle
    Array<UniquePtr<ComponentInterface>> m_ownedInterfaces;

    // Every table ever published stays alive until the registry is destroyed; readers never lock
    Array<UniquePtr<LookupTable>> m_lookupTables;
    AtomicVar<const LookupTable*> m_lookupTable;
    AtomicVar<uint32> m_runtimeRegistrationGeneration;

    Mutex m_writeMutex;

    bool m_isInitialized;
};

template <class ComponentType, bool ShouldSerialize = true, bool ShowInEditor = true>
struct ComponentInterfaceRegistration
{
    static_assert(!std::is_base_of_v<ObjectBase, ComponentType>, "Components must be HYP_STRUCT types");

    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<ComponentType>(),
            []() -> UniquePtr<ComponentInterface>
            {
                return ComponentInterface::CreateForStruct(&TypeOf<ComponentType>(), sizeof(ComponentType), alignof(ComponentType), ShouldSerialize);
            });
    }
};

template <EntityTag Tag, bool ShouldSerialize, bool ShowInEditor>
struct ComponentInterfaceRegistration<TagComponent<Tag>, ShouldSerialize, ShowInEditor>
{
    static_assert(std::is_base_of_v<TagComponentBase, TagComponent<Tag>>);
    static_assert(sizeof(TagComponent<Tag>) == sizeof(TagComponentBase) && alignof(TagComponent<Tag>) == alignof(TagComponentBase),
        "TagComponent must be layout-identical to TagComponentBase, as it shares its Struct");

    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<TagComponent<Tag>>(),
            []() -> UniquePtr<ComponentInterface>
            {
                return ComponentInterface::CreateForEntityTag(&TypeOf<TagComponent<Tag>>(), sizeof(TagComponent<Tag>), alignof(TagComponent<Tag>), Tag, ShouldSerialize, ShowInEditor);
            });
    }
};

template <class T, bool ShouldSerialize, bool ShowInEditor>
struct ComponentInterfaceRegistration<EntityTypeTag<T>, ShouldSerialize, ShowInEditor>
{
    static constexpr EntityTag Tag = EntityType_Impl<T>::value;

    static_assert(std::is_base_of_v<TagComponentBase, EntityTypeTag<T>>);
    static_assert(sizeof(EntityTypeTag<T>) == sizeof(TagComponentBase) && alignof(EntityTypeTag<T>) == alignof(TagComponentBase),
        "EntityTypeTag must be layout-identical to TagComponentBase, as it shares its Struct");

    ComponentInterfaceRegistration()
    {
        ComponentInterfaceRegistry::GetInstance().Register(
            TypeId::ForType<EntityTypeTag<T>>(),
            []() -> UniquePtr<ComponentInterface>
            {
                return ComponentInterface::CreateForEntityTag(&TypeOf<EntityTypeTag<T>>(), sizeof(EntityTypeTag<T>), alignof(EntityTypeTag<T>), Tag, ShouldSerialize, ShowInEditor);
            });
    }
};

#define HYP_REGISTER_COMPONENT(type, ...)                                                             \
    static ComponentInterfaceRegistration<type, ##__VA_ARGS__> type##_ComponentInterface_Registration \
    {                                                                                                 \
    }
#define HYP_REGISTER_ENTITY_TAG(tag, ...)                                                                                              \
    static ComponentInterfaceRegistration<TagComponent<EntityTag::tag>, ##__VA_ARGS__> tag##_EntityTag_ComponentInterface_Registration \
    {                                                                                                                                  \
    }

#define HYP_REGISTER_ENTITY_TYPE(T, ...)                                                                                                               \
    static ComponentInterfaceRegistration<EntityTypeTag<T>, false, false, ##__VA_ARGS__> T##_EntityTag_ComponentInterface_Registration \
    {                                                                                                                                                  \
    }

} // namespace Hyperion
