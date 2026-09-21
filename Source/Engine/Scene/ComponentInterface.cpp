/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ComponentInterface.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

namespace Hyperion {

extern "C" ENGINE_API const ClassAttribute* Class_GetAttribute(const Class* cls, const Name* name);

static const ClassAttributeValue& GetComponentClassAttributeValue(const Class* cls, const Name& name)
{
    const ClassAttribute* attribute = Class_GetAttribute(cls, &name);

    if (!attribute)
    {
        return ClassAttributeValue::empty;
    }

    return attribute->GetValue();
}

#pragma region ComponentInterface

ComponentInterface::ComponentInterface(const TypeInfo* typeInfo, const Struct* componentStruct, EnumFlags<ComponentInterfaceFlags> flags, EntityTag entityTag)
    : m_typeInfo(typeInfo),
      m_typeId(typeInfo != nullptr ? typeInfo->id : TypeId::Void()),
      m_struct(componentStruct),
      m_structReference(componentStruct),
      m_entityTag(entityTag),
      m_flags(flags)
{
    Assert(m_typeInfo != nullptr);
    Assert(m_struct != nullptr, "Component type {} has no Struct", m_typeInfo->name);
    Assert(m_struct->CanConstructInPlace() && m_struct->CanMoveConstructInPlace(),
        "Component type {} must be default and move constructible in place", m_typeInfo->name);
}

ComponentInterface::~ComponentInterface() = default;

UniquePtr<ComponentInterface> ComponentInterface::CreateForStruct(const TypeInfo* typeInfo, size_t expectedSize, size_t expectedAlignment, bool shouldSerialize)
{
    Assert(typeInfo != nullptr);

    const Class* cls = typeInfo->GetClass();
    const Struct* componentStruct = GetStructFromClass(cls);

    Assert(componentStruct != nullptr, "Component type {} is not a HYP_STRUCT", typeInfo->name);
    Assert(!componentStruct->IsDynamic());
    Assert(componentStruct->GetSize() == expectedSize && componentStruct->GetAlignment() == expectedAlignment,
        "Struct layout for component type {} does not match the C++ type", typeInfo->name);

    EnumFlags<ComponentInterfaceFlags> flags = ComponentInterfaceFlags::NONE;

    // a missing attribute compares unequal to false, so serialize/replicated default to on
    if (shouldSerialize && GetComponentClassAttributeValue(cls, Attributes::g_attrSerialize) != false)
    {
        flags |= ComponentInterfaceFlags::SERIALIZE | ComponentInterfaceFlags::SHOW_IN_EDITOR;
    }

    if (GetComponentClassAttributeValue(cls, Attributes::g_attrReplicated) != false)
    {
        flags |= ComponentInterfaceFlags::REPLICATED;
    }

    return MakeUnique<ComponentInterface>(typeInfo, componentStruct, flags);
}

UniquePtr<ComponentInterface> ComponentInterface::CreateForEntityTag(const TypeInfo* typeInfo, size_t expectedSize, size_t expectedAlignment, EntityTag entityTag, bool shouldSerialize, bool showInEditor)
{
    Assert(typeInfo != nullptr);

    const Struct* tagBaseStruct = GetStructFromClass(TagComponentBase::StaticClass());

    Assert(tagBaseStruct != nullptr, "TagComponentBase has no Struct");
    Assert(tagBaseStruct->GetSize() == expectedSize && tagBaseStruct->GetAlignment() == expectedAlignment,
        "Tag component type {} is not layout-identical to TagComponentBase", typeInfo->name);

    EnumFlags<ComponentInterfaceFlags> flags = ComponentInterfaceFlags::ENTITY_TAG;

    if (shouldSerialize)
    {
        flags |= ComponentInterfaceFlags::SERIALIZE | ComponentInterfaceFlags::REPLICATED;

        if (showInEditor)
        {
            flags |= ComponentInterfaceFlags::SHOW_IN_EDITOR;
        }
    }

    return MakeUnique<ComponentInterface>(typeInfo, tagBaseStruct, flags, entityTag);
}

const Class* ComponentInterface::GetClass() const
{
    return m_typeInfo->GetClass();
}

size_t ComponentInterface::GetComponentSize() const
{
    return m_struct->GetSize();
}

size_t ComponentInterface::GetComponentAlignment() const
{
    return m_struct->GetAlignment();
}

void ComponentInterface::ConstructComponent(void* destination) const
{
    m_struct->ConstructInPlace(destination);

    if (IsEntityTag())
    {
        static_cast<TagComponentBase*>(destination)->value = m_entityTag;
    }
}

void ComponentInterface::CopyConstructComponent(void* destination, const void* source) const
{
    m_struct->CopyConstructInPlace(destination, source);
}

void ComponentInterface::MoveConstructComponent(void* destination, void* source) const
{
    m_struct->MoveConstructInPlace(destination, source);
}

void ComponentInterface::DestructComponent(void* target) const
{
    m_struct->DestructInPlace(target);
}

bool ComponentInterface::CreateInstance(BoxedValue& out) const
{
    if (IsEntityTag())
    {
        if (!m_struct->ConstructBoxed(out, m_typeInfo))
        {
            return false;
        }

        static_cast<TagComponentBase*>(out.ToRef().GetPointer())->value = m_entityTag;

        return true;
    }

    if (!m_struct->CanCreateInstance())
    {
        return false;
    }

    return m_struct->CreateInstance(out);
}

void ComponentInterface::ReleaseStructReference()
{
    m_structReference = ClassRef();
}

#pragma endregion ComponentInterface

#pragma region ComponentInterfaceRegistry

ComponentInterfaceRegistry& ComponentInterfaceRegistry::GetInstance()
{
    static ComponentInterfaceRegistry s_instance;

    return s_instance;
}

ComponentInterfaceRegistry::ComponentInterfaceRegistry()
    : m_lookupTable(nullptr),
      m_isInitialized(false)
{
}

ComponentInterfaceRegistry::~ComponentInterfaceRegistry()
{
    m_lookupTable.Set(nullptr, MemoryOrder::RELEASE);
}

void ComponentInterfaceRegistry::Initialize()
{
    Mutex::Guard guard(m_writeMutex);

    Assert(!m_isInitialized, "Component interface registry already initialized!");

    UniquePtr<LookupTable> lookupTable = MakeUnique<LookupTable>();

    for (auto& it : m_nativeCreateFunctions)
    {
        UniquePtr<ComponentInterface> componentInterface = it.second();
        Assert(componentInterface != nullptr);
        Assert(componentInterface->GetTypeId() == it.first);

        AddToLookupTable(*lookupTable, componentInterface.Get());

        m_ownedInterfaces.PushBack(std::move(componentInterface));
    }

    PublishLookupTable(std::move(lookupTable));

    m_isInitialized = true;
}

void ComponentInterfaceRegistry::Shutdown()
{
    Mutex::Guard guard(m_writeMutex);

    if (!m_isInitialized)
    {
        return;
    }

    m_lookupTable.Set(nullptr, MemoryOrder::RELEASE);

    // containers hold their own Struct references, so dynamic structs outlive this if components are still alive
    for (const UniquePtr<ComponentInterface>& componentInterface : m_ownedInterfaces)
    {
        componentInterface->ReleaseStructReference();
    }

    m_isInitialized = false;
}

void ComponentInterfaceRegistry::Register(TypeId typeId, UniquePtr<ComponentInterface> (*createFunction)())
{
    m_nativeCreateFunctions.Set(typeId, createFunction);
}

const ComponentInterface* ComponentInterfaceRegistry::RegisterRuntimeComponent(const Struct* componentStruct, EnumFlags<ComponentInterfaceFlags> flags)
{
    if (!componentStruct)
    {
        return nullptr;
    }

    const TypeInfo* typeInfo = componentStruct->GetTypeInfo();
    Assert(typeInfo != nullptr);

    const TypeId typeId = componentStruct->GetTypeId();

    if (!componentStruct->IsDynamic() || !typeId.IsDynamicType())
    {
        HYP_LOG(Entity, Error, "Cannot register runtime component '{}': its Struct must be dynamic", componentStruct->GetName());

        return nullptr;
    }

    const size_t size = componentStruct->GetSize();
    const size_t alignment = componentStruct->GetAlignment();

    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0 || alignment > 64)
    {
        HYP_LOG(Entity, Error, "Cannot register runtime component '{}': invalid size ({}) or alignment ({})", componentStruct->GetName(), size, alignment);

        return nullptr;
    }

    if (!componentStruct->CanConstructInPlace() || !componentStruct->CanMoveConstructInPlace())
    {
        HYP_LOG(Entity, Error, "Cannot register runtime component '{}': it must be default and move constructible in place", componentStruct->GetName());

        return nullptr;
    }

    // entity tags are native only
    flags &= ~(ComponentInterfaceFlags::ENTITY_TAG);
    flags |= ComponentInterfaceFlags::RUNTIME;

    Mutex::Guard guard(m_writeMutex);

    if (!m_isInitialized)
    {
        HYP_LOG(Entity, Error, "Cannot register runtime component '{}': the component interface registry is not initialized", componentStruct->GetName());

        return nullptr;
    }

    auto existingIt = GetLookupTable().interfacesByTypeId.Find(typeId);

    if (existingIt != GetLookupTable().interfacesByTypeId.End())
    {
        // registering the same Struct again (eg a reloaded script assembly) keeps the existing registration
        if (existingIt->second->GetStruct() == componentStruct)
        {
            return existingIt->second;
        }

        HYP_LOG(Entity, Error, "Cannot register runtime component '{}': a different component with TypeId {} is already registered", componentStruct->GetName(), typeId.Value());

        return nullptr;
    }

    UniquePtr<ComponentInterface> componentInterface = MakeUnique<ComponentInterface>(typeInfo, componentStruct, flags);
    const ComponentInterface* result = componentInterface.Get();

    UniquePtr<LookupTable> lookupTable = CopyLookupTable();
    AddToLookupTable(*lookupTable, result);

    m_ownedInterfaces.PushBack(std::move(componentInterface));

    PublishLookupTable(std::move(lookupTable));

    return result;
}

bool ComponentInterfaceRegistry::UnregisterRuntimeComponent(TypeId typeId)
{
    Mutex::Guard guard(m_writeMutex);

    if (!m_isInitialized)
    {
        return false;
    }

    const LookupTable& currentLookupTable = GetLookupTable();

    auto it = currentLookupTable.interfacesByTypeId.Find(typeId);

    if (it == currentLookupTable.interfacesByTypeId.End() || !it->second->IsRuntimeComponent())
    {
        return false;
    }

    ComponentInterface* componentInterface = const_cast<ComponentInterface*>(it->second);

    UniquePtr<LookupTable> lookupTable = MakeUnique<LookupTable>();

    for (const auto& existingIt : currentLookupTable.interfacesByTypeId)
    {
        if (existingIt.second != componentInterface)
        {
            AddToLookupTable(*lookupTable, existingIt.second);
        }
    }

    PublishLookupTable(std::move(lookupTable));

    componentInterface->ReleaseStructReference();

    return true;
}

const ComponentInterface* ComponentInterfaceRegistry::GetComponentInterface(TypeId typeId) const
{
    const LookupTable& lookupTable = GetLookupTable();

    auto it = lookupTable.interfacesByTypeId.Find(typeId);

    if (it == lookupTable.interfacesByTypeId.End())
    {
        return nullptr;
    }

    return it->second;
}

const ComponentInterface* ComponentInterfaceRegistry::GetEntityTagComponentInterface(EntityTag tag) const
{
    const LookupTable& lookupTable = GetLookupTable();

    auto it = lookupTable.interfacesByEntityTag.Find(uint64(tag));

    if (it == lookupTable.interfacesByEntityTag.End())
    {
        return nullptr;
    }

    return it->second;
}

Array<const ComponentInterface*> ComponentInterfaceRegistry::GetComponentInterfaces() const
{
    const LookupTable& lookupTable = GetLookupTable();

    Array<const ComponentInterface*> interfaces;
    interfaces.Reserve(lookupTable.interfacesByTypeId.Size());

    for (const auto& it : lookupTable.interfacesByTypeId)
    {
        interfaces.PushBack(it.second);
    }

    return interfaces;
}

const ComponentInterfaceRegistry::LookupTable& ComponentInterfaceRegistry::GetLookupTable() const
{
    const LookupTable* lookupTable = m_lookupTable.Get(MemoryOrder::ACQUIRE);
    Assert(lookupTable != nullptr, "Component interface registry not initialized!");

    return *lookupTable;
}

UniquePtr<ComponentInterfaceRegistry::LookupTable> ComponentInterfaceRegistry::CopyLookupTable() const
{
    UniquePtr<LookupTable> lookupTable = MakeUnique<LookupTable>();

    for (const auto& it : GetLookupTable().interfacesByTypeId)
    {
        AddToLookupTable(*lookupTable, it.second);
    }

    return lookupTable;
}

void ComponentInterfaceRegistry::PublishLookupTable(UniquePtr<LookupTable>&& lookupTable)
{
    Assert(lookupTable != nullptr);

    m_lookupTable.Set(lookupTable.Get(), MemoryOrder::RELEASE);
    m_lookupTables.PushBack(std::move(lookupTable));
}

void ComponentInterfaceRegistry::AddToLookupTable(LookupTable& lookupTable, const ComponentInterface* componentInterface)
{
    Assert(componentInterface != nullptr);

    lookupTable.interfacesByTypeId.Set(componentInterface->GetTypeId(), componentInterface);

    if (componentInterface->IsEntityTag())
    {
        // first registration wins, matching the linear scan this replaces
        const uint64 tagValue = uint64(componentInterface->GetEntityTag());

        if (!lookupTable.interfacesByEntityTag.Contains(tagValue))
        {
            lookupTable.interfacesByEntityTag.Set(tagValue, componentInterface);
        }
    }
}

#pragma endregion ComponentInterfaceRegistry

} // namespace Hyperion
