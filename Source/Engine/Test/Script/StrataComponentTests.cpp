/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#if defined(HYP_TESTS) && defined(HYP_STRATA) && defined(HYP_STRATA_JIT)

#include <Scripting/StrataTypes.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/TypeId.hpp>
#include <Core/Reflection/GenericArrayWrapper.hpp>

#include <Core/Scripting/Strata/ThunkDrawer.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>

#include <Asset/SerializationUtils.hpp>

#include <strata/strata.h>

namespace Hyperion {
namespace tests {
namespace script {

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

const char* const g_firstLayoutSource = R"(
handle Entity;

struct TestStrataStats { float armor = 2.5; int level = 3; };

@component
struct TestStrataHealth
{
    float current = 100.0;
    TestStrataStats stats;
    TestStrataStats[2] history;
    int[3] slots;
    Entity owner;
};

extern<T> bool GetComponent(Entity entity, ref T value);
extern<T> void SetComponent(Entity entity, const ref T value);
extern<T> bool HasComponent(Entity entity);
extern<T> bool RemoveComponent(Entity entity);

float damage(Entity entity, float amount)
{
    TestStrataHealth health;
    GetComponent(entity, health);
    health.current = health.current - amount;
    health.history[1].level = 9;
    health.slots[2] = 4;
    health.owner = entity;
    SetComponent(entity, health);
    return health.current;
}

bool has(Entity entity) { return HasComponent<TestStrataHealth>(entity); }
)";

// the same types after an edit: a field added before `level`, and a new field on the component
const char* const g_secondLayoutSource = R"(
@component
struct TestStrataHealth
{
    float current = 100.0;
    float shield = 5.0;
    TestStrataStats stats;
    TestStrataStats[2] history;
    int[3] slots;
};

struct TestStrataStats { float armor = 2.5; float regen = 1.0; int level = 3; };
)";

StrataJit* CompileAndBind(StrataCompiler* compiler, const char* source)
{
    const char* err = nullptr;
    StrataJit* jit = strataJitCompileString(compiler, source, "StrataComponentTests", &err);

    if (!jit)
    {
        HYP_LOG(Engine, Error, "Strata compile failed: {}", err ? err : "(no message)");

        if (err)
        {
            strataFree(const_cast<char*>(err));
        }

        return nullptr;
    }

    for (size_t externIndex = 0; externIndex < strataJitGetExternSymbolCount(jit); externIndex++)
    {
        const char* name = strataJitGetExternSymbolName(jit, externIndex);

        if (void* hostFunction = Strata::ThunkDrawer::Resolve(StringHash(name)))
        {
            strataJitAddSymbol(jit, name, hostFunction);
        }
    }

    size_t metadataSize = 0;

    if (const void* metadata = strataJitGetTypeMetadata(jit, &metadataSize))
    {
        Strata::RegisterTypeMetadata(metadata, metadataSize, FilePath("StrataComponentTests"));
    }

    return jit;
}

const Struct* StructNamed(const char* name)
{
    return GetStructFromClass(GetClass(TypeId::ForManagedType(name)));
}

template <class T>
T PropertyValue(const Struct* owner, const char* propertyName, const BoxedValue& target)
{
    const Property* property = owner ? owner->GetProperty(CreateNameFromDynamicString(propertyName)) : nullptr;

    if (!property)
    {
        return T {};
    }

    const BoxedValue boxed = property->Get(target);
    auto value = boxed.TryGet<T>();

    return value.HasValue() ? T(*value) : T {};
}

void TestRegistrationAndAccess(EntityManager* entityManager)
{
    StrataCompiler* compiler = strataCompilerCreate();
    StrataJit* jit = CompileAndBind(compiler, g_firstLayoutSource);

    Check("Strata: script compiles and binds", jit != nullptr);

    if (!jit)
    {
        strataCompilerDestroy(compiler);
        return;
    }

    const TypeId healthTypeId = TypeId::ForManagedType("TestStrataHealth");
    const Struct* healthStruct = StructNamed("TestStrataHealth");
    const Struct* statsStruct = StructNamed("TestStrataStats");

    Check("Strata: component and nested struct register", healthStruct != nullptr && statsStruct != nullptr);
    Check("Strata: the component type registers", ComponentInterfaceRegistry::GetInstance().GetComponentInterface(healthTypeId) != nullptr);

    entityManager->SyncRuntimeComponentTypes();

    Handle<Entity> entity = entityManager->AddEntity();

    auto damage = (float (*)(Entity*, float))strataJitGetFunction(jit, "damage");
    auto has = (bool (*)(Entity*))strataJitGetFunction(jit, "has");

    Check("Strata: script functions resolve", damage != nullptr && has != nullptr);

    if (!damage || !has || !healthStruct || !statsStruct)
    {
        strataJitDestroy(jit);
        strataCompilerDestroy(compiler);
        return;
    }

    Check("Strata: HasComponent is false before the script adds it", !has(entity.Get()));

    const float afterDamage = damage(entity.Get(), 10.0f);

    Check("Strata: SetComponent adds the component, starting from its defaults", afterDamage == 90.0f, HYP_FORMAT("{}", afterDamage));
    Check("Strata: HasComponent sees the added component", has(entity.Get()));

    BoxedValue component(entityManager->TryGetComponent(healthTypeId, entity.Get()));

    Check("Strata: the component is readable through its properties", PropertyValue<float>(healthStruct, "current", component) == 90.0f);

    BoxedValue stats = healthStruct->GetProperty(NAME("stats"))->Get(component);

    Check("Strata: nested struct fields keep their own defaults", PropertyValue<float>(statsStruct, "armor", stats) == 2.5f);

    BoxedValue history = healthStruct->GetProperty(NAME("history"))->Get(component);
    const bool historyIsArray = history.IsArray();

    Check("Strata: fixed arrays are reflected as arrays", historyIsArray);

    if (historyIsArray)
    {
        GenericArrayWrapper& historyElements = history.Get<GenericArrayWrapper>();
        BoxedValue lastEntry;

        Check("Strata: fixed arrays have their declared length and can't be resized", historyElements.Size() == 2 && !historyElements.CanResize() && !historyElements.CanPushBack());
        Check("Strata: array elements carry script writes", historyElements.GetElementAt(1, lastEntry) && PropertyValue<int32>(statsStruct, "level", lastEntry) == 9);
    }

    const Property* ownerProperty = healthStruct->GetProperty(NAME("owner"));

    Check("Strata: handle fields are transient", ownerProperty != nullptr && ownerProperty->GetAttribute("transient"_sh).GetBool());

    // saving and loading goes through the same properties
    String savedText;
    ObjectToHMF(entity->InstanceClass(), BoxedValue(entity), savedText);

    Check("Strata: the component and its nested values are saved", savedText.Contains("TestStrataHealth") && savedText.Contains("history"), savedText);
    Check("Strata: handle fields aren't saved", !savedText.Contains("owner"), savedText);

    Handle<Entity> loadedEntity = entityManager->AddEntity();
    BoxedValue loadedBoxed(loadedEntity);

    HMF::ParseResult parseResult = HMF::Parse(savedText, nullptr, &loadedBoxed);

    Check("Strata: the saved entity parses", !parseResult.HasError(), parseResult.HasError() ? parseResult.GetError().GetMessage() : String());

    BoxedValue loadedComponent(entityManager->TryGetComponent(healthTypeId, loadedEntity.Get()));

    if (loadedComponent.ToRef().HasValue())
    {
        BoxedValue loadedHistory = healthStruct->GetProperty(NAME("history"))->Get(loadedComponent);
        BoxedValue loadedLastEntry;

        Check("Strata: loaded values match the saved ones",
            PropertyValue<float>(healthStruct, "current", loadedComponent) == 90.0f
                && loadedHistory.IsArray() && loadedHistory.Get<GenericArrayWrapper>().GetElementAt(1, loadedLastEntry)
                && PropertyValue<int32>(statsStruct, "level", loadedLastEntry) == 9);
    }
    else
    {
        Check("Strata: the loaded entity has the component", false);
    }

    // an edit changes both layouts: live components migrate, nested values included
    void* secondMetadata = nullptr;
    size_t secondMetadataSize = 0;
    const char* err = nullptr;

    Check("Strata: the edited types compile", strataCompileTypeMetadataString(compiler, g_secondLayoutSource, "StrataComponentTests", &secondMetadata, &secondMetadataSize, &err));

    if (secondMetadata)
    {
        Strata::RegisterTypeMetadata(secondMetadata, secondMetadataSize, FilePath("StrataComponentTests"));
        strataFreeTypeMetadata(secondMetadata);
    }

    entityManager->SyncRuntimeComponentTypes();

    const Struct* migratedHealthStruct = StructNamed("TestStrataHealth");
    const Struct* migratedStatsStruct = StructNamed("TestStrataStats");

    Check("Strata: a changed layout replaces the struct", migratedHealthStruct != nullptr && migratedHealthStruct != healthStruct);

    BoxedValue migrated(entityManager->TryGetComponent(healthTypeId, entity.Get()));

    if (migrated.ToRef().HasValue() && migratedHealthStruct && migratedStatsStruct)
    {
        BoxedValue migratedHistory = migratedHealthStruct->GetProperty(NAME("history"))->Get(migrated);
        BoxedValue migratedLastEntry;
        const bool readEntry = migratedHistory.IsArray() && migratedHistory.Get<GenericArrayWrapper>().GetElementAt(1, migratedLastEntry);

        Check("Strata: migrated components keep same-named fields", PropertyValue<float>(migratedHealthStruct, "current", migrated) == 90.0f);
        Check("Strata: new fields take their defaults", PropertyValue<float>(migratedHealthStruct, "shield", migrated) == 5.0f);
        Check("Strata: nested structs migrate field by field",
            readEntry && PropertyValue<int32>(migratedStatsStruct, "level", migratedLastEntry) == 9
                && PropertyValue<float>(migratedStatsStruct, "regen", migratedLastEntry) == 1.0f);
    }
    else
    {
        Check("Strata: the migrated component is readable", false);
    }

    // the old script was compiled against the old layout, so the engine refuses its component access
    Check("Strata: a script compiled against an old layout is refused", !has(entity.Get()));

    entityManager->RemoveComponent(healthTypeId, entity.Get());
    entityManager->RemoveComponent(healthTypeId, loadedEntity.Get());

    strataJitDestroy(jit);
    strataCompilerDestroy(compiler);
}

} // namespace

ENGINE_API void RunStrataComponentTests()
{
    g_passCount = 0;
    g_failCount = 0;

    {
        Handle<Scene> scene = MakeHandle<Scene>(NAME("StrataComponentTestScene"), ThreadId::Current(), SceneFlags::NONE);
        InitObject(scene);

        TestRegistrationAndAccess(scene->GetEntityManager().Get());
    }

    HYP_LOG(Engine, Info, "========== Strata Component Test Results: {} passed, {} failed ==========", g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! STRATA COMPONENT TEST HAD FAILURES !!!");
    }
}

} // namespace script
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS && HYP_STRATA && HYP_STRATA_JIT
