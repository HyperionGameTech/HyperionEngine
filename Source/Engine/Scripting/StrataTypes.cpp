/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Scripting/StrataTypes.hpp>

#ifdef HYP_STRATA

#    include <Scene/Entity.hpp>
#    include <Scene/EntityManager.hpp>
#    include <Scene/ComponentInterface.hpp>

#    include <Core/Reflection/Struct.hpp>
#    include <Core/Reflection/TypeInfo.hpp>

#    include <Core/Containers/Map.hpp>

#    include <Core/Threading/Mutex.hpp>
#    include <Core/Threading/AtomicVar.hpp>

#    include <Core/Math/Vector2.hpp>
#    include <Core/Math/Vector3.hpp>
#    include <Core/Math/Vector4.hpp>

#    include <Core/Scripting/Strata/ThunkDrawer.hpp>

#    include <Core/Logging/Logger.hpp>
#    include <Core/Logging/LogChannels.hpp>

#    include <strata/strata.h>
#    include <strata/strata_types.h>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Scripting);

namespace Strata {

struct RegisteredStrataType
{
    // one reference, released when a changed layout replaces the struct
    DynamicStructInstance* structInstance = nullptr;
    uint64 layoutHash = 0;
    bool isComponent = false;
};

static Mutex g_registeredTypesMutex;
static Map<Name, RegisteredStrataType> g_registeredTypes;

// bumped whenever a registration changes what a type name resolves to; invalidates ResolveComponentType's caches
static AtomicVar<uint32> g_registrationGeneration { 1 };

static const TypeInfo* ScalarTypeInfo(StrataTypeKind kind)
{
    switch (kind)
    {
    case STRATA_TYPE_BOOL:
        return &TypeOf<bool>();
    case STRATA_TYPE_I8:
        return &TypeOf<int8>();
    case STRATA_TYPE_U8:
        return &TypeOf<uint8>();
    case STRATA_TYPE_I16:
        return &TypeOf<int16>();
    case STRATA_TYPE_U16:
        return &TypeOf<uint16>();
    case STRATA_TYPE_I32:
        return &TypeOf<int32>();
    case STRATA_TYPE_U32:
        return &TypeOf<uint32>();
    case STRATA_TYPE_I64:
        return &TypeOf<int64>();
    case STRATA_TYPE_U64:
        return &TypeOf<uint64>();
    case STRATA_TYPE_F32:
        return &TypeOf<float>();
    case STRATA_TYPE_F64:
        return &TypeOf<double>();
    case STRATA_TYPE_FLOAT2:
        return &TypeOf<Vec2f>();
    case STRATA_TYPE_FLOAT3:
        return &TypeOf<Vec3f>();
    case STRATA_TYPE_FLOAT4:
        return &TypeOf<Vec4f>();
    default:
        return nullptr;
    }
}

// The field as the engine reflects it, or false for a field it can't describe.
static bool MakeFieldDesc(const StrataTypes& types, const StrataTypesField& field, Span<const Struct* const> registeredStructs, DynamicStructFieldDesc& outDesc)
{
    outDesc.name = CreateNameFromDynamicString(field.name);
    outDesc.offset = field.offset;
    outDesc.size = field.size;
    outDesc.arrayLength = field.arrayLength;

    switch (field.kind)
    {
    case STRATA_TYPE_STRUCT:
        if (field.typeIndex >= registeredStructs.Size() || !registeredStructs[field.typeIndex])
        {
            return false;
        }

        outDesc.typeInfo = registeredStructs[field.typeIndex]->GetTypeInfo();
        return true;

    case STRATA_TYPE_ENUM:
    {
        // stored as the underlying integer; the enum's names aren't reflected yet
        StrataTypesEnum enumInfo;

        if (!strataTypesGetEnum(&types, field.typeIndex, &enumInfo))
        {
            return false;
        }

        outDesc.typeInfo = ScalarTypeInfo(enumInfo.underlyingKind);
        return outDesc.typeInfo != nullptr;
    }

    case STRATA_TYPE_HANDLE:
        // an engine object pointer: survives a layout change, but can't be saved
        outDesc.typeInfo = &TypeOf<uint64>();
        outDesc.isTransient = true;
        return true;

    default:
        outDesc.typeInfo = ScalarTypeInfo(field.kind);
        return outDesc.typeInfo != nullptr;
    }
}

bool RegisterTypeMetadata(const void* data, size_t size, const FilePath& sourcePath)
{
    StrataTypes types;

    if (!strataTypesOpen(&types, data, size))
    {
        HYP_LOG(Scripting, Error, "Strata: type metadata from '{}' is unreadable (corrupt, or from an incompatible compiler)", sourcePath);

        return false;
    }

    Mutex::Guard guard(g_registeredTypesMutex);

    bool changedRegistrations = false;

    // structs are listed after the structs they hold by value, so each nested type is ready first
    Array<const Struct*> registeredStructs;
    registeredStructs.Reserve(strataTypesStructCount(&types));

    for (uint32 structIndex = 0; structIndex < strataTypesStructCount(&types); structIndex++)
    {
        registeredStructs.PushBack(nullptr);
    }

    for (uint32 structIndex = 0; structIndex < strataTypesStructCount(&types); structIndex++)
    {
        StrataTypesStruct info;

        if (!strataTypesGetStruct(&types, structIndex, &info))
        {
            HYP_LOG(Scripting, Error, "Strata: struct {} in the type metadata from '{}' is unreadable", structIndex, sourcePath);

            continue;
        }

        const Name name = CreateNameFromDynamicString(info.name);
        const bool isComponent = (info.flags & STRATA_TYPES_STRUCT_COMPONENT) != 0;

        RegisteredStrataType* existing = nullptr;

        if (auto it = g_registeredTypes.Find(name); it != g_registeredTypes.End())
        {
            existing = &it->second;
        }

        if (existing && existing->layoutHash == info.layoutHash)
        {
            // unchanged layout: keep the Struct (and every component built with it), but pick up edited defaults
            existing->structInstance->SetDefaultValue(info.defaultValue);
            registeredStructs[structIndex] = existing->structInstance;

            if (isComponent && !existing->isComponent)
            {
                existing->isComponent = ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(existing->structInstance) != nullptr;
                changedRegistrations = true;
            }

            continue;
        }

        Array<DynamicStructFieldDesc> fields;
        fields.Reserve(info.fieldCount);

        for (uint32 fieldIndex = 0; fieldIndex < info.fieldCount; fieldIndex++)
        {
            StrataTypesField field;
            DynamicStructFieldDesc fieldDesc;

            if (!strataTypesGetField(&types, &info, fieldIndex, &field)
                || !MakeFieldDesc(types, field, registeredStructs.ToSpan(), fieldDesc))
            {
                HYP_LOG(Scripting, Warning, "Strata: field {} of '{}' has a type the engine can't reflect; it won't be saved or shown in the editor",
                    fieldIndex, name);

                continue;
            }

            fields.PushBack(fieldDesc);
        }

        const TypeId typeId = TypeId::ForManagedType(info.name);

        if (!existing)
        {
            if (const Class* otherDefinition = GetClass(typeId); otherDefinition && otherDefinition->IsDynamic())
            {
                HYP_LOG(Scripting, Warning, "Strata: '{}' from '{}' replaces a script type of the same name defined elsewhere (another script language?)",
                    name, sourcePath);
            }
        }

        DynamicStructDesc desc;
        desc.typeId = typeId;
        desc.name = name;
        desc.size = info.size;
        desc.alignment = info.alignment;
        desc.defaultValue = info.defaultValue;
        desc.fields = fields.ToSpan();

        DynamicStructInstance* structInstance = CreateDynamicStruct(desc);

        if (!structInstance)
        {
            HYP_LOG(Scripting, Error, "Strata: failed to register struct '{}' from '{}'", name, sourcePath);

            continue;
        }

        if (existing)
        {
            // components and boxes built with the old definition keep it alive until they migrate
            existing->structInstance->Release();
        }

        RegisteredStrataType& registered = g_registeredTypes[name];
        registered.structInstance = structInstance;
        registered.layoutHash = info.layoutHash;
        registered.isComponent = false;

        registeredStructs[structIndex] = structInstance;
        changedRegistrations = true;

        if (isComponent)
        {
            // replaces the previous definition's interface; containers migrate on the next SyncRuntimeComponentTypes
            registered.isComponent = ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(structInstance) != nullptr;

            if (!registered.isComponent)
            {
                HYP_LOG(Scripting, Error, "Strata: failed to register component type '{}' from '{}'", name, sourcePath);
            }
        }
    }

    if (changedRegistrations)
    {
        g_registrationGeneration.Increment(1, MemoryOrder::RELEASE);
    }

    return true;
}

#    pragma region Component access

// Slow path of ResolveComponentType: the registry lookup, under the registration lock.
static const ComponentInterface* LookupComponentType(const StrataTypeDesc* type)
{
    const char* typeName = strataTypeDescName(type);

    {
        Mutex::Guard guard(g_registeredTypesMutex);

        auto it = g_registeredTypes.Find(CreateNameFromDynamicString(typeName));

        if (it == g_registeredTypes.End() || !it->second.isComponent)
        {
            HYP_LOG(Scripting, Error, "Strata: '{}' isn't a registered component type; declare it with @component", typeName);

            return nullptr;
        }

        if (it->second.layoutHash != type->layoutHash)
        {
            HYP_LOG(Scripting, Error, "Strata: a script was compiled against a different layout of '{}' than the one registered; recompile it", typeName);

            return nullptr;
        }
    }

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(TypeId::ForManagedType(typeName));

    if (!componentInterface || componentInterface->GetComponentSize() != type->size)
    {
        HYP_LOG(Scripting, Error, "Strata: component type '{}' isn't registered with the size the script expects", typeName);

        return nullptr;
    }

    return componentInterface;
}

struct ResolvedComponentType
{
    const StrataTypeDesc* type = nullptr;
    uint64 nameHash = 0;
    uint64 layoutHash = 0;
    const ComponentInterface* componentInterface = nullptr; // nullptr caches a failed lookup, so its error logs once
};

// Resolutions made since registrations last changed. Descriptors live in script code, so a
// destroyed script's address can be reused by another; the hashes tell them apart.
static thread_local Array<ResolvedComponentType> t_resolvedComponentTypes;
static thread_local uint64 t_resolvedComponentTypesGeneration = 0;

// reloading scripts without changing any type leaves descriptors of destroyed scripts behind
static constexpr size_t g_maxResolvedComponentTypes = 256;

// The component type a script names through `extern<T>`, checked against the layout the script was compiled with.
// Scripts call this for every component access, so it only takes the registration lock when registrations changed.
static const ComponentInterface* ResolveComponentType(const StrataTypeDesc* type)
{
    if (!type)
    {
        return nullptr;
    }

    // another script language replacing a component interface invalidates the cache too
    const uint64 generation = (uint64(g_registrationGeneration.Get(MemoryOrder::ACQUIRE)) << 32)
        | ComponentInterfaceRegistry::GetInstance().GetRuntimeRegistrationGeneration();

    if (generation != t_resolvedComponentTypesGeneration || t_resolvedComponentTypes.Size() >= g_maxResolvedComponentTypes)
    {
        t_resolvedComponentTypes.Clear();
        t_resolvedComponentTypesGeneration = generation;
    }

    for (const ResolvedComponentType& resolved : t_resolvedComponentTypes)
    {
        if (resolved.type == type && resolved.nameHash == type->nameHash && resolved.layoutHash == type->layoutHash)
        {
            return resolved.componentInterface;
        }
    }

    ResolvedComponentType& resolved = t_resolvedComponentTypes.EmplaceBack();
    resolved.type = type;
    resolved.nameHash = type->nameHash;
    resolved.layoutHash = type->layoutHash;
    resolved.componentInterface = LookupComponentType(type);

    return resolved.componentInterface;
}

extern "C"
{
    // extern<T> bool GetComponent(Entity entity, ref T value): copies the entity's component into `value`
    static bool Strata_GetComponent(Entity* entity, void* value, const StrataTypeDesc* type)
    {
        const ComponentInterface* componentInterface = ResolveComponentType(type);

        if (!componentInterface || !entity || !value || !entity->GetEntityManager())
        {
            return false;
        }

        AnyRef component = entity->GetEntityManager()->TryGetComponent(componentInterface->GetTypeId(), entity);

        if (!component.HasValue())
        {
            return false;
        }

        Memory::Copy(value, component.GetPointer(), type->size);

        return true;
    }

    ///overwrites the entity's component, adding it if missing
    /// Strata extern declaration: extern<T> void SetComponent(Entity entity, const ref T value)
    static void Strata_SetComponent(Entity* entity, const void* value, const StrataTypeDesc* type)
    {
        const ComponentInterface* componentInterface = ResolveComponentType(type);

        if (!componentInterface || !entity || !value || !entity->GetEntityManager())
        {
            return;
        }

        EntityManager* entityManager = entity->GetEntityManager();

        AnyRef component = entityManager->TryGetComponent(componentInterface->GetTypeId(), entity);

        if (component.HasValue())
        {
            Memory::Copy(component.GetPointer(), value, type->size);

            return;
        }

        entityManager->AddComponent(entity, BoxedValue(AnyRef(&componentInterface->GetTypeInfo(), const_cast<void*>(value))));
    }

    // extern<T> bool HasComponent(Entity entity)
    static bool Strata_HasComponent(Entity* entity, const StrataTypeDesc* type)
    {
        const ComponentInterface* componentInterface = ResolveComponentType(type);

        if (!componentInterface || !entity || !entity->GetEntityManager())
        {
            return false;
        }

        return entity->GetEntityManager()->HasComponent(componentInterface->GetTypeId(), entity);
    }

    // extern<T> bool RemoveComponent(Entity entity)
    static bool Strata_RemoveComponent(Entity* entity, const StrataTypeDesc* type)
    {
        const ComponentInterface* componentInterface = ResolveComponentType(type);

        if (!componentInterface || !entity || !entity->GetEntityManager())
        {
            return false;
        }

        return entity->GetEntityManager()->RemoveComponent(componentInterface->GetTypeId(), entity);
    }
} // extern "C"

static const bool s_strataBinding_GetComponent = ThunkDrawer::Register("GetComponent"_sh, (void*)&Strata_GetComponent);
static const bool s_strataBinding_SetComponent = ThunkDrawer::Register("SetComponent"_sh, (void*)&Strata_SetComponent);
static const bool s_strataBinding_HasComponent = ThunkDrawer::Register("HasComponent"_sh, (void*)&Strata_HasComponent);
static const bool s_strataBinding_RemoveComponent = ThunkDrawer::Register("RemoveComponent"_sh, (void*)&Strata_RemoveComponent);

#    pragma endregion Component access

} // namespace Strata
} // namespace Hyperion

#endif // HYP_STRATA
