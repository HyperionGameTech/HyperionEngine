/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Core/Containers/StridedBuffer.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/MemberVariant.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/TypeId.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector3.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <cstring>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

int g_passCount = 0;
int g_failCount = 0;

void Check(const char* testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

bool IsAligned(const void* pointer, size_t alignment)
{
    return (uintptr_t(pointer) & (alignment - 1)) == 0;
}

#pragma region Runtime component

// Stands in for a script-defined component: 48 bytes, 16 byte aligned, filled with a pattern derived from its serial
static constexpr uint32 g_runtimeComponentSize = 48;
static constexpr uint32 g_runtimeComponentAlignment = 16;
static constexpr uint32 g_liveMagic = 0xC0FFEE01u;
static constexpr uint32 g_movedFromMagic = 0x0DEAD0DEu;
static constexpr uint32 g_destroyedMagic = 0xDDDDDDDDu;

struct RuntimeComponentCounters
{
    uint32 constructs = 0;
    uint32 copies = 0;
    uint32 moves = 0;
    uint32 destructs = 0;
    uint32 invalidDestructs = 0;
    uint32 nextSerial = 1;
};

RuntimeComponentCounters g_counters;

uint8 GetPatternByte(uint32 serial, uint32 byteIndex)
{
    return uint8((serial * 31u + byteIndex * 7u) & 0xFFu);
}

void WriteRuntimeComponent(void* destination, uint32 serial)
{
    ubyte* bytes = static_cast<ubyte*>(destination);

    std::memcpy(bytes, &g_liveMagic, sizeof(uint32));
    std::memcpy(bytes + 4, &serial, sizeof(uint32));

    for (uint32 byteIndex = 8; byteIndex < g_runtimeComponentSize; byteIndex++)
    {
        bytes[byteIndex] = GetPatternByte(serial, byteIndex);
    }
}

uint32 ReadMagic(const void* component)
{
    uint32 magic;
    std::memcpy(&magic, component, sizeof(uint32));

    return magic;
}

uint32 ReadSerial(const void* component)
{
    uint32 serial;
    std::memcpy(&serial, static_cast<const ubyte*>(component) + 4, sizeof(uint32));

    return serial;
}

bool IsValidRuntimeComponent(const void* component)
{
    if (!component || ReadMagic(component) != g_liveMagic)
    {
        return false;
    }

    const uint32 serial = ReadSerial(component);
    const ubyte* bytes = static_cast<const ubyte*>(component);

    for (uint32 byteIndex = 8; byteIndex < g_runtimeComponentSize; byteIndex++)
    {
        if (bytes[byteIndex] != GetPatternByte(serial, byteIndex))
        {
            return false;
        }
    }

    return true;
}

void RuntimeComponent_Construct(void* context, void* destination)
{
    ++g_counters.constructs;

    WriteRuntimeComponent(destination, g_counters.nextSerial++);
}

void RuntimeComponent_CopyConstruct(void* context, void* destination, const void* source)
{
    ++g_counters.copies;

    std::memcpy(destination, source, g_runtimeComponentSize);
}

void RuntimeComponent_MoveConstruct(void* context, void* destination, void* source)
{
    ++g_counters.moves;

    std::memcpy(destination, source, g_runtimeComponentSize);
    std::memcpy(source, &g_movedFromMagic, sizeof(uint32));
}

void RuntimeComponent_Destruct(void* context, void* target)
{
    ++g_counters.destructs;

    const uint32 magic = ReadMagic(target);

    if (magic != g_liveMagic && magic != g_movedFromMagic)
    {
        ++g_counters.invalidDestructs;
    }

    std::memcpy(target, &g_destroyedMagic, sizeof(uint32));
}

DynamicStructInstance* CreateRuntimeComponentStruct(TypeId typeId)
{
    DynamicStructInstanceFunctions functions {};
    functions.construct = &RuntimeComponent_Construct;
    functions.destruct = &RuntimeComponent_Destruct;
    functions.copyConstruct = &RuntimeComponent_CopyConstruct;
    functions.moveConstruct = &RuntimeComponent_MoveConstruct;

    return new DynamicStructInstance(
        typeId,
        NAME("TestRuntimeComponent"),
        g_runtimeComponentSize,
        g_runtimeComponentAlignment,
        Span<const ClassAttribute>(),
        ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
        Span<MemberVariant>(),
        functions);
}

#pragma endregion Runtime component

void TestStridedBuffer()
{
    StridedBuffer<DynamicAllocator> buffer(24, 64, 4);

    Check("StridedBuffer: block size rounded to alignment", buffer.GetBlockSize() == 64 && buffer.GetAlignment() == 64);

    const uint32 indices[] = { 0, 5, 17, 3, 100, 9 };
    ubyte* pointers[6] = {};

    bool allAligned = true;

    for (uint32 i = 0; i < 6; i++)
    {
        pointers[i] = buffer.AllocateElementRaw(indices[i]);
        allAligned &= IsAligned(pointers[i], 64);

        std::memcpy(pointers[i], &indices[i], sizeof(uint32));
    }

    Check("StridedBuffer: sparse allocations are aligned", allAligned);

    bool stable = true;

    for (uint32 i = 0; i < 6; i++)
    {
        uint32 value = 0;
        std::memcpy(&value, buffer.GetElementRaw(indices[i]), sizeof(uint32));

        stable &= buffer.GetElementRaw(indices[i]) == pointers[i] && value == indices[i];
    }

    Check("StridedBuffer: addresses and contents stable across slabs", stable);
    Check("StridedBuffer: unallocated index is empty", !buffer.HasIndex(4) && buffer.GetElementRaw(4) == nullptr && buffer.GetElementRaw(5000) == nullptr);

    buffer.FreeElementRaw(5);
    Check("StridedBuffer: freed index is empty", !buffer.HasIndex(5));

    ubyte* reallocated = buffer.AllocateElementRaw(5);
    Check("StridedBuffer: reallocated index is aligned", reallocated != nullptr && IsAligned(reallocated, 64) && buffer.HasIndex(5));

    uint32 visitedCount = 0;
    buffer.ForEachElementRaw([&visitedCount](size_t, ubyte*)
        {
            ++visitedCount;
        });

    Check("StridedBuffer: ForEachElementRaw visits every element", visitedCount == 6 && buffer.NumActiveAllocations() == 6);

    buffer.Reset();
    Check("StridedBuffer: Reset clears everything", !buffer.HasIndex(0) && buffer.NumActiveAllocations() == 0);
}

void TestDynamicStructDefaultValue()
{
    struct DefaultValueLayout
    {
        uint32 first;
        float second;
        uint64 third;
    };

    const DefaultValueLayout defaultValue { 7, 1.5f, 0x1122334455667788ull };

    DynamicStructInstance* dynamicStruct = new DynamicStructInstance(
        TypeId::ForManagedType("TestDefaultValueStruct"),
        NAME("TestDefaultValueStruct"),
        sizeof(DefaultValueLayout),
        alignof(DefaultValueLayout),
        Span<const ClassAttribute>(),
        ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
        Span<MemberVariant>(),
        DynamicStructInstanceFunctions {});

    dynamicStruct->SetDefaultValue(&defaultValue);

    {
        BoxedValue constructed;
        const bool didConstruct = dynamicStruct->ConstructBoxed(constructed);

        Check("DynamicStruct: default construction copies the default value template",
            didConstruct && std::memcmp(constructed.ToRef().GetPointer(), &defaultValue, sizeof(DefaultValueLayout)) == 0);
    }

    dynamicStruct->Release();
}

void TestDynamicStructFields()
{
    // a float followed by a Vec3f at offset 4, less aligned than Vec3f normally is, as C# lays it out
    static constexpr uint32 structSize = sizeof(float) + sizeof(Vec3f);

    DynamicStructFieldDesc speedField;
    speedField.name = NAME("Speed");
    speedField.offset = 0;
    speedField.size = sizeof(float);
    speedField.typeInfo = &TypeOf<float>();

    DynamicStructFieldDesc directionField;
    directionField.name = NAME("Direction");
    directionField.offset = sizeof(float);
    directionField.size = sizeof(Vec3f);
    directionField.typeInfo = &TypeOf<Vec3f>();

    DynamicStructFieldDesc mismatchedField;
    mismatchedField.name = NAME("Mismatched");
    mismatchedField.offset = 0;
    mismatchedField.size = 2;
    mismatchedField.typeInfo = &TypeOf<float>();

    DynamicStructFieldDesc unsupportedField;
    unsupportedField.name = NAME("Unsupported");
    unsupportedField.offset = 0;
    unsupportedField.size = sizeof(String);
    unsupportedField.typeInfo = &TypeOf<String>();

    MemberVariant members[2];

    Check("DynamicStruct fields: supported fields become properties",
        MakeDynamicStructProperty(speedField, 0, members[0]) && MakeDynamicStructProperty(directionField, 1, members[1]));

    MemberVariant rejectedMember;
    Check("DynamicStruct fields: size mismatch is rejected", !MakeDynamicStructProperty(mismatchedField, 2, rejectedMember));
    Check("DynamicStruct fields: types without a trivially copyable Struct are rejected", !MakeDynamicStructProperty(unsupportedField, 2, rejectedMember));

    DynamicStructInstance* dynamicStruct = new DynamicStructInstance(
        TypeId::ForManagedType("TestFieldsStruct"),
        NAME("TestFieldsStruct"),
        structSize,
        alignof(float),
        Span<const ClassAttribute>(),
        ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
        Span<MemberVariant>(members, 2),
        DynamicStructInstanceFunctions {});

    {
        const Property* speedProperty = dynamicStruct->GetProperty("Speed"_sh);
        const Property* directionProperty = dynamicStruct->GetProperty("Direction"_sh);

        Check("DynamicStruct fields: properties are found by name", speedProperty != nullptr && directionProperty != nullptr);

        BoxedValue instance;

        if (speedProperty && directionProperty && dynamicStruct->ConstructBoxed(instance))
        {
            speedProperty->Set(instance, BoxedValue(2.5f));
            directionProperty->Set(instance, BoxedValue(Vec3f(1.0f, 2.0f, 3.0f)));

            Check("DynamicStruct fields: values round trip through the properties",
                speedProperty->Get(instance).Get<float>() == 2.5f
                    && directionProperty->Get(instance).Get<Vec3f>() == Vec3f(1.0f, 2.0f, 3.0f));

            const float expectedDirection[] = { 1.0f, 2.0f, 3.0f };
            const ubyte* bytes = static_cast<const ubyte*>(instance.ToRef().GetPointer());

            Check("DynamicStruct fields: values are stored at their offsets",
                std::memcmp(bytes + sizeof(float), expectedDirection, sizeof(expectedDirection)) == 0);
        }
    }

    dynamicStruct->Release();
}

void TestNativeAndTagComponents(EntityManager* entityManagerA, const Handle<EntityManager>& entityManagerB)
{
    Handle<Entity> entity = entityManagerA->AddEntity();

    TransformComponent transformComponent;
    transformComponent.translation = Vec3f(1.0f, 2.0f, 3.0f);
    transformComponent.scale = Vec3f(4.0f);

    TransformComponent& addedTransform = entityManagerA->AddComponent<TransformComponent>(entity.Get(), transformComponent);
    Check("Native: typed add returns stored component", addedTransform.translation == Vec3f(1.0f, 2.0f, 3.0f));
    Check("Native: typed get", entityManagerA->GetComponent<TransformComponent>(entity.Get()).scale == Vec3f(4.0f));

    AnyRef transformRef = entityManagerA->TryGetComponent(TypeId::ForType<TransformComponent>(), entity.Get());
    Check("Native: erased get returns the native TypeInfo", transformRef.HasValue() && transformRef.GetTypeInfo() == &TypeOf<TransformComponent>());
    Check("Native: erased and typed get agree", transformRef.GetPointer() == &entityManagerA->GetComponent<TransformComponent>(entity.Get()));

    uint32 entitySetCount = 0;
    for (auto [setEntity, setTransform] : entityManagerA->GetEntitySet<TransformComponent>().GetScopedView(DataAccessFlags::ACCESS_READ))
    {
        if (setEntity == entity.Get() && setTransform.translation == Vec3f(1.0f, 2.0f, 3.0f))
        {
            ++entitySetCount;
        }
    }

    Check("Native: EntitySet iteration sees the component", entitySetCount == 1);

    entityManagerA->AddTag(entity.Get(), EntityTag::Player);
    Check("Tag: HasTag after AddTag", entityManagerA->HasTag(entity.Get(), EntityTag::Player) && entityManagerA->HasTag<EntityTag::Player>(entity.Get()));
    Check("Tag: default construction stamps the tag value", entityManagerA->GetComponent<TagComponent<EntityTag::Player>>(entity.Get()).value == EntityTag::Player);

    entityManagerA->MoveEntity(entity, entityManagerB);

    Check("Move: entity now belongs to the other manager", entity->GetEntityManager() == entityManagerB.Get());
    Check("Move: native component value preserved", entityManagerB->HasComponent<TransformComponent>(entity.Get())
        && entityManagerB->GetComponent<TransformComponent>(entity.Get()).translation == Vec3f(1.0f, 2.0f, 3.0f));
    Check("Move: tag preserved", entityManagerB->HasTag(entity.Get(), EntityTag::Player)
        && entityManagerB->GetComponent<TagComponent<EntityTag::Player>>(entity.Get()).value == EntityTag::Player);

    Check("Remove: typed RemoveComponent", entityManagerB->RemoveComponent<TransformComponent>(entity.Get()) && !entityManagerB->HasComponent<TransformComponent>(entity.Get()));
    Check("Remove: RemoveTag", entityManagerB->RemoveTag(entity.Get(), EntityTag::Player) && !entityManagerB->HasTag(entity.Get(), EntityTag::Player));
}

void TestRuntimeComponent(EntityManager* entityManagerA, const Handle<EntityManager>& entityManagerB)
{
    g_counters = RuntimeComponentCounters {};

    const TypeId runtimeTypeId = TypeId::ForManagedType("TestRuntimeComponent");

    DynamicStructInstance* runtimeStruct = CreateRuntimeComponentStruct(runtimeTypeId);

    Check("Runtime: dynamic struct TypeInfo has the real size", runtimeStruct->GetTypeInfo()->size == g_runtimeComponentSize
        && runtimeStruct->GetTypeInfo()->alignment == g_runtimeComponentAlignment);

    // registered after both scenes exist, so their containers are created on first use
    const ComponentInterface* runtimeInterface = ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(runtimeStruct);
    Check("Runtime: registration succeeds", runtimeInterface != nullptr && runtimeInterface->IsRuntimeComponent());
    Check("Runtime: registering the same Struct again returns the existing registration", ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(runtimeStruct) == runtimeInterface);
    Check("Runtime: IsValidComponentType", EntityManager::IsValidComponentType(runtimeTypeId));

    if (!runtimeInterface)
    {
        runtimeStruct->Release();

        return;
    }

    {
        Handle<Entity> entity = entityManagerA->AddEntity();
        entityManagerA->AddDefaultComponent(entity.Get(), runtimeTypeId);

        AnyRef componentRef = entityManagerA->TryGetComponent(runtimeTypeId, entity.Get());
        Check("Runtime: default constructed component is valid", IsValidRuntimeComponent(componentRef.GetPointer()));
        Check("Runtime: component storage honours alignment", IsAligned(componentRef.GetPointer(), g_runtimeComponentAlignment));
        Check("Runtime: AnyRef reports the runtime type", componentRef.GetTypeId() == runtimeTypeId);

        const uint32 originalSerial = ReadSerial(componentRef.GetPointer());

        // copy through CreateInstance + the const BoxedValue path
        {
            BoxedValue createdInstance;
            Check("Runtime: CreateInstance", runtimeInterface->CreateInstance(createdInstance) && IsValidRuntimeComponent(createdInstance.ToRef().GetPointer()));

            BoxedValue copiedBox = createdInstance;
            Check("Runtime: boxed copy is a deep copy", copiedBox.ToRef().GetPointer() != createdInstance.ToRef().GetPointer()
                && IsValidRuntimeComponent(copiedBox.ToRef().GetPointer()));

            Handle<Entity> copyTarget = entityManagerA->AddEntity();
            entityManagerA->AddComponent(copyTarget.Get(), createdInstance);

            AnyRef copiedRef = entityManagerA->TryGetComponent(runtimeTypeId, copyTarget.Get());
            Check("Runtime: component copied from BoxedValue", IsValidRuntimeComponent(copiedRef.GetPointer())
                && ReadSerial(copiedRef.GetPointer()) == ReadSerial(createdInstance.ToRef().GetPointer()));
        }

        // clone through the reflected Components property, the path saving, loading and Entity::Clone use
        {
            const Property* componentsProperty = entity->InstanceClass()->GetProperty("Components"_sh);
            Check("Runtime: entity exposes its Components property", componentsProperty != nullptr);

            if (componentsProperty)
            {
                const BoxedValue serializedComponents = componentsProperty->Get(BoxedValue(entity));

                Handle<Entity> cloneTarget = entityManagerA->AddEntity();
                BoxedValue cloneTargetBoxed(cloneTarget);
                componentsProperty->Set(cloneTargetBoxed, serializedComponents);

                AnyRef clonedRef = entityManagerA->TryGetComponent(runtimeTypeId, cloneTarget.Get());
                Check("Runtime: serialize/deserialize keeps component bytes", clonedRef.HasValue()
                    && std::memcmp(clonedRef.GetPointer(), componentRef.GetPointer(), g_runtimeComponentSize) == 0);
            }
        }

        // enough entities to span several slabs; remove every other component and add them back
        {
            static constexpr uint32 numEntities = 3000;

            Array<Handle<Entity>> entities;
            Array<const void*> addresses;
            entities.Reserve(numEntities);
            addresses.Reserve(numEntities);

            for (uint32 i = 0; i < numEntities; i++)
            {
                Handle<Entity> bulkEntity = entityManagerA->AddEntity();
                entityManagerA->AddDefaultComponent(bulkEntity.Get(), runtimeTypeId);

                addresses.PushBack(entityManagerA->TryGetComponent(runtimeTypeId, bulkEntity.Get()).GetPointer());
                entities.PushBack(std::move(bulkEntity));
            }

            bool removedAll = true;

            for (uint32 i = 0; i < numEntities; i += 2)
            {
                removedAll &= entityManagerA->RemoveComponent(runtimeTypeId, entities[i].Get());
            }

            Check("Runtime: removing every other component", removedAll);

            bool survivorsIntact = true;

            for (uint32 i = 1; i < numEntities; i += 2)
            {
                const void* component = entityManagerA->TryGetComponent(runtimeTypeId, entities[i].Get()).GetPointer();
                survivorsIntact &= component == addresses[i] && IsValidRuntimeComponent(component);
            }

            Check("Runtime: surviving components keep their address and contents", survivorsIntact);

            bool readdedValid = true;

            for (uint32 i = 0; i < numEntities; i += 2)
            {
                entityManagerA->AddDefaultComponent(entities[i].Get(), runtimeTypeId);

                const void* component = entityManagerA->TryGetComponent(runtimeTypeId, entities[i].Get()).GetPointer();
                readdedValid &= IsValidRuntimeComponent(component) && IsAligned(component, g_runtimeComponentAlignment);
            }

            Check("Runtime: re-added components are valid and aligned", readdedValid);
        }

        // move between managers
        {
            Handle<Entity> movedEntity = entityManagerA->AddEntity();
            entityManagerA->AddDefaultComponent(movedEntity.Get(), runtimeTypeId);

            const uint32 movedSerial = ReadSerial(entityManagerA->TryGetComponent(runtimeTypeId, movedEntity.Get()).GetPointer());

            entityManagerA->MoveEntity(movedEntity, entityManagerB);

            AnyRef movedRef = entityManagerB->TryGetComponent(runtimeTypeId, movedEntity.Get());
            Check("Runtime: component survives MoveEntity", IsValidRuntimeComponent(movedRef.GetPointer()) && ReadSerial(movedRef.GetPointer()) == movedSerial);
        }

        Check("Runtime: original component untouched", IsValidRuntimeComponent(componentRef.GetPointer()) && ReadSerial(componentRef.GetPointer()) == originalSerial);
    }

    Check("Runtime: no destructor saw a corrupted component", g_counters.invalidDestructs == 0);

    // Unregistering while a component is alive: its destructor must still run when the entity goes away
    {
        Handle<Entity> liveEntity = entityManagerA->AddEntity();
        entityManagerA->AddDefaultComponent(liveEntity.Get(), runtimeTypeId);

        Check("Runtime: unregister", ComponentInterfaceRegistry::GetInstance().UnregisterRuntimeComponent(runtimeTypeId));
        Check("Runtime: unregistered type is no longer valid", !EntityManager::IsValidComponentType(runtimeTypeId));

        const uint32 destructsBefore = g_counters.destructs;
        liveEntity.Reset();

        Check("Runtime: component destroyed after its type was unregistered", g_counters.destructs == destructsBefore + 1);
    }

    // entityManagerA's container is now empty, so the type can be registered again
    {
        const ComponentInterface* reregisteredInterface = ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(runtimeStruct);
        Check("Runtime: re-registration after unregister", reregisteredInterface != nullptr);

        if (reregisteredInterface)
        {
            Handle<Entity> entity = entityManagerA->AddEntity();
            entityManagerA->AddDefaultComponent(entity.Get(), runtimeTypeId);

            Check("Runtime: re-registered type usable", IsValidRuntimeComponent(entityManagerA->TryGetComponent(runtimeTypeId, entity.Get()).GetPointer()));

            ComponentInterfaceRegistry::GetInstance().UnregisterRuntimeComponent(runtimeTypeId);
        }
    }

    runtimeStruct->Release();
}

// A saved component whose class isn't registered yet (eg its script hasn't loaded) is kept, written back, and restored later
void TestUnresolvedComponents(EntityManager* entityManager)
{
    Handle<Entity> entity = entityManager->AddEntity();
    BoxedValue entityBoxed(entity);

    HMF::ParseResult parseResult = HMF::Parse(String("Entity { Components = [ TestUnresolvedComponent { Value = 2.5 } ] }"), nullptr, &entityBoxed);

    Check("Unresolved: entity with an unknown component class still parses", !parseResult.HasError(),
        parseResult.HasError() ? parseResult.GetError().GetMessage() : String());
    Check("Unresolved: the component is kept", entity->HasUnresolvedComponents());

    String savedText;
    ObjectToHMF(entity->InstanceClass(), BoxedValue(entity), savedText);

    Check("Unresolved: the component is written back when saving",
        savedText.Contains("TestUnresolvedComponent") && savedText.Contains("2.5"), savedText);

    DynamicStructFieldDesc valueField;
    valueField.name = NAME("Value");
    valueField.offset = 0;
    valueField.size = sizeof(float);
    valueField.typeInfo = &TypeOf<float>();

    MemberVariant members[1];
    MakeDynamicStructProperty(valueField, 0, members[0]);

    const TypeId typeId = TypeId::ForManagedType("TestUnresolvedComponent");

    DynamicStructInstance* dynamicStruct = new DynamicStructInstance(
        typeId,
        NAME("TestUnresolvedComponent"),
        sizeof(float),
        alignof(float),
        Span<const ClassAttribute>(),
        ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
        Span<MemberVariant>(members, 1),
        DynamicStructInstanceFunctions {});

    Check("Unresolved: registering its class", ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(dynamicStruct) != nullptr);

    entityManager->ResolveUnresolvedComponents();

    AnyRef componentRef = entityManager->TryGetComponent(typeId, entity.Get());

    float restoredValue = 0.0f;

    if (componentRef.HasValue())
    {
        std::memcpy(&restoredValue, componentRef.GetPointer(), sizeof(float));
    }

    Check("Unresolved: component restored once its class registers", componentRef.HasValue() && !entity->HasUnresolvedComponents());
    Check("Unresolved: restored component keeps its saved value", restoredValue == 2.5f);

    entityManager->RemoveComponent(typeId, entity.Get());
    ComponentInterfaceRegistry::GetInstance().UnregisterRuntimeComponent(typeId);
    dynamicStruct->Release();
}

} // namespace

ENGINE_API void RunComponentContainerTests()
{
    g_passCount = 0;
    g_failCount = 0;

    TestStridedBuffer();
    TestDynamicStructDefaultValue();
    TestDynamicStructFields();

    {
        Handle<Scene> sceneA = MakeHandle<Scene>(NAME("ComponentContainerTestSceneA"), ThreadId::Current(), SceneFlags::NONE);
        Handle<Scene> sceneB = MakeHandle<Scene>(NAME("ComponentContainerTestSceneB"), ThreadId::Current(), SceneFlags::NONE);

        InitObject(sceneA);
        InitObject(sceneB);

        TestNativeAndTagComponents(sceneA->GetEntityManager().Get(), sceneB->GetEntityManager());
        TestRuntimeComponent(sceneA->GetEntityManager().Get(), sceneB->GetEntityManager());
        TestUnresolvedComponents(sceneA->GetEntityManager().Get());
    }

    Check("Runtime: every construct/copy/move is matched by a destruct once the scenes are gone",
        g_counters.constructs + g_counters.copies + g_counters.moves == g_counters.destructs,
        HYP_FORMAT("constructs={} copies={} moves={} destructs={}", g_counters.constructs, g_counters.copies, g_counters.moves, g_counters.destructs));

    HYP_LOG(Engine, Info, "========== Component Container Test Results: {} passed, {} failed ==========", g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! COMPONENT CONTAINER TEST HAD FAILURES !!!");
    }
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
