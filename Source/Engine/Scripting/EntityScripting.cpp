/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Scripting/EntityScripting.hpp>

#include <Scene/Entity.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/ScriptComponent.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Asset/AssetRegistry.hpp>

#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/DotNETHost.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Method.hpp>

#include <Framework/Game.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorTask.hpp>
#endif // HYP_EDITOR

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif // HYP_SCRIPT

#ifdef HYP_STRATA_JIT
#include <strata/strata.h>

#include <Core/Scripting/Strata/ThunkDrawer.hpp>
#endif // HYP_STRATA_JIT

#if HYP_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif HYP_UNIX
#  include <dlfcn.h>
#endif

#include <System/MessageBox.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Scripting);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

#ifdef HYP_STRATA

namespace Strata {

static Pool s_strataPool { 1 * 1024 * 1024, PF_THREAD_SAFE | PF_FALLBACK };

extern "C"
{
    void* strata_alloc(size_t count)
    {
        return s_strataPool.Allocate(count);
    }

    static void strata_free(void* ptr)
    {
        s_strataPool.Free(ptr);
    }
} // extern "C"

// thread-local cache for strata module -> function pointer map
struct FunctionPointerCache
{
    using FunctionMap = Map<StringHash, void*>;
    using ModuleMap = Map<StringHash, FunctionMap>;

    ModuleMap modules;

    void* TryGet(StringHash moduleHash, StringHash functionHash) const
    {
        auto moduleIt = modules.Find(moduleHash);
        if (moduleIt == modules.End())
        {
            return nullptr;
        }

        auto functionIt = moduleIt->second.Find(functionHash);
        if (functionIt == moduleIt->second.End())
        {
            return nullptr;
        }

        return functionIt->second;
    }

    void Put(StringHash moduleHash, StringHash functionHash, void* fnPtr)
    {
        if (!fnPtr)
        {
            return;
        }

        modules[moduleHash][functionHash] = fnPtr;
    }

    void ClearModule(StringHash moduleHash)
    {
        modules.Erase(moduleHash);
    }
};

thread_local FunctionPointerCache* t_fnPtrCache = nullptr;

static void ShutdownCache()
{
    delete t_fnPtrCache;
    t_fnPtrCache = nullptr;
}

static void InitializeCache()
{
    if (t_fnPtrCache != nullptr)
    {
        return;
    }

    t_fnPtrCache = new FunctionPointerCache;

    if (ThreadBase* currThread = CurrentThreadObject())
    {
        currThread->AddOnExitCallback(ShutdownCache);
    }
}

// @TODO Remove when we use static linkage instead
static void* ResolveSymbolFromHost(const char* name)
{
    if (name == nullptr || *name == '\0')
    {
        return nullptr;
    }

    // @TODO a more elegant way of doing this...
    if (strcmp(name, "printf") == 0)
    {
        return &printf;
    }

#if HYP_WINDOWS
    if (HMODULE h = GetModuleHandleW(nullptr))
    {
        return reinterpret_cast<void*>(GetProcAddress(h, name));
    }

    return nullptr;
#elif HYP_UNIX
    // RTLD_DEFAULT searches the main program and all globally-loaded objects.
    return dlsym(RTLD_DEFAULT, name);
#else
    return nullptr;
#endif
}

static void* ResolveFunctionPointer(ScriptObjectData_Strata* data, const char* name)
{
    Assert(data != nullptr);

    const StringHash functionHash(name);

    if (void* cached = t_fnPtrCache->TryGet(data->moduleHash, functionHash))
    {
        return cached;
    }

    void* fn = nullptr;

#ifdef HYP_STRATA_JIT
    if (data->jit != nullptr)
    {
        fn = strataJitGetFunction(data->jit, name);
    }
#endif // HYP_STRATA_JIT

    if (fn == nullptr)
    {
        fn = ResolveSymbolFromHost(name);
    }

    t_fnPtrCache->Put(data->moduleHash, functionHash, fn);

    return fn;
}

#ifdef HYP_STRATA_JIT

thread_local StrataCompiler* t_strataCompiler = nullptr;

void ShutdownCompiler()
{
    if (t_strataCompiler != nullptr)
    {
        strataCompilerDestroy(t_strataCompiler);
        t_strataCompiler = nullptr;
    }

    ShutdownCache();
}

void InitializeCompiler()
{
    InitializeCache();

    if (t_strataCompiler == nullptr)
    {
        t_strataCompiler = strataCompilerCreate();
        Assert(t_strataCompiler != nullptr);

        strataJitSetAllocFreeFunctions(t_strataCompiler, (void*) &Strata::strata_alloc, (void*) &Strata::strata_free);

        if (ThreadBase* currThread = CurrentThreadObject())
        {
            currThread->AddOnExitCallback(ShutdownCompiler);
        }
    }
}

static const Map<ANSIStringView, void*> s_globalFunctions = {
    { "printf", (void*)&printf },
    { "puts", (void*)&puts },
    { "putchar", (void*)&putchar },
    { "memset", (void*)&memset },
    { "memcpy", (void*)&memcpy },
    { "memcmp", (void*)&memcmp }
};

void BindExterns(StrataJit* jit)
{
    Assert(jit != nullptr);

    const size_t externCount = strataJitGetExternSymbolCount(jit);

    for (size_t i = 0; i < externCount; ++i)
    {
        const char* name = strataJitGetExternSymbolName(jit, i);

        if (void* hostFn = ThunkDrawer::Resolve(StringHash(name)))
        {
            strataJitAddSymbol(jit, name, hostFn);

            continue;
        }

        // Try global functions next:
        auto it = s_globalFunctions.Find(ANSIStringView(name));
        if (it != s_globalFunctions.End())
        {
            if (strataJitAddSymbol(jit, name, it->second) == 1)
            {
                continue;
            }

            HYP_LOG(Scripting, Error, "Failed to bind global function {}", name);
        }

        HYP_LOG(Scripting, Error, "Strata: no host binding for extern '{}'. Any call to this function will result in a crash!", name);
    }
}

#endif // HYP_STRATA_JIT

} // namespace Strata

#endif // HYP_STRATA

namespace EntityScripting {

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, const ArgTypes&... args)
{
    Assert(sor != nullptr);

    const uint32 mask = sor->GetScriptLanguageMask();

#ifdef HYP_DOTNET
    if (mask & (1u << uint32(ScriptLanguage::CSharp)))
    {
        AssertDebug(sor->GetManagedObject() != nullptr);

        if (dotnet::ManagedClass* managedClass = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::ManagedMethod* managedMethod = managedClass->GetMethod(methodName))
            {
                if (!managedMethod->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented

                    if constexpr (!std::is_void_v<ReturnType>)
                    {
                        AssertDebug(outReturnValue != nullptr);

                        new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(managedMethod, args...));
                    }
                    else
                    {
                        sor->GetManagedObject()->InvokeMethod<void>(managedMethod, args...);
                    }
                }
            }
        }
    }
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
    if (mask & (1u << uint32(ScriptLanguage::HypScript)))
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        namespace HS = HypScript;

        BoxedValue functionValue;
        if (HS::GetFunctionHandle(data->instance, methodName, functionValue))
        {
            const size_t numArgs = sizeof...(ArgTypes);

            FixedArray<BoxedValue, sizeof...(ArgTypes)> argsArray { BoxedValue(args)... };

            BoxedValue returnValue = HS::CallFunctionArgV(data->instance, functionValue, argsArray.Data(), static_cast<uint8>(argsArray.Size()));

            if constexpr (!std::is_void_v<ReturnType>)
            {
                Assert(returnValue.IsValid());
                Assert(returnValue.Is<ReturnType>());

                AssertDebug(outReturnValue != nullptr);

                // we construct the return value in place
                new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
            }
        }
    }
#endif // HYP_SCRIPT

#ifdef HYP_STRATA
    if (mask & (1u << uint32(ScriptLanguage::Strata)))
    {
        auto* data = sor->GetScriptObjectData_Strata();
        Assert(data != nullptr);

        Strata::InitializeCache();

        if (void* fnPtrRaw = Strata::ResolveFunctionPointer(data, methodName))
        {
            auto fnPtrCasted = (ReturnType (*)(ArgTypes...))fnPtrRaw;

            if constexpr (!std::is_void_v<ReturnType>)
            {
                AssertDebug(outReturnValue != nullptr);

                new (outReturnValue) ReturnType(fnPtrCasted(args...));
            }
            else
            {
                fnPtrCasted(args...);
            }
        }
    }
#endif // HYP_STRATA

    if (mask & (1u << uint32(ScriptLanguage::Native)))
    {
        auto* data = sor->GetScriptObjectData_Native();
        Assert(data != nullptr);

        Handle<ObjectBase> nativeObject = data->nativeObject.Lock();
        AssertDebug(nativeObject != nullptr);

        if (const Method* method = nativeObject->InstanceClass()->GetMethod(StringHash(methodName)))
        {
            if constexpr (!std::is_void_v<ReturnType>)
            {
                AssertDebug(outReturnValue != nullptr);
                new (outReturnValue) ReturnType(method->Invoke(Span<BoxedValue> { { BoxedValue(nativeObject), BoxedValue(args)... } }));
            }
            else
            {
                (void)method->Invoke(Span<BoxedValue> { { BoxedValue(nativeObject), BoxedValue(args)... } });
            }
        }
    }
}

static HYP_FORCE_INLINE void InvokeScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    InvokeScriptMethodT<void>(nullptr, target.scriptObjectResource, *methodName);
}

void InitializeEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState)
{
    World* world = entity->GetWorld();
    Scene* scene = entity->GetScene();

    ScriptObjectResource*& sor = scriptComponent.scriptObjectResource;

    if (scriptComponent.nativeObject != nullptr) // native script object
    {
        if (!sor || !sor->GetScriptObjectData_Native() || sor->GetScriptObjectData_Native()->nativeObject.GetUnsafe() != scriptComponent.nativeObject.Get())
        {
            delete sor;
            sor = nullptr;
        }

        sor = new ScriptObjectResource(scriptComponent.nativeObject);
        sor->AddReader();

        const Class* nativeClass = scriptComponent.nativeObject->InstanceClass();
        AssertDebug(nativeClass != nullptr);

        HYP_LOG(Scripting, Verbose, "Created ScriptObjectResource for ScriptComponent, native class: {}", nativeClass->GetName());

        InitObject(scriptComponent.nativeObject);

        if (!gameState.IsStopped())
        {
            if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED))
            {
                InvokeScriptMethodT<void>(nullptr, sor, "BeforeAdded", world, scene);
                InvokeScriptMethodT<void>(nullptr, sor, "OnAdded", entity);

                scriptComponent.flags |= ScriptComponentFlags::ACTIVATED;
            }
        }
    }
    else // external script object (C# or HypScript)
    {
        const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;

        if (!scriptAsset)
        {
            HYP_LOG(Scripting, Warning, "Entity has ScriptComponent with no ScriptAsset!");

            return;
        }

        ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

        switch (scriptDesc.language)
        {
#ifdef HYP_DOTNET
        case ScriptLanguage::CSharp:
        {
            if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
            {
                delete sor;
                sor = nullptr;

                auto readScope = scriptAsset->GetReadScope();

                if (!scriptComponent.assembly)
                {
                    ANSIString assemblyPath(scriptDesc.assemblyPath.Data(), scriptDesc.assemblyPath.Data() + scriptDesc.assemblyPath.Size());

                    if (scriptDesc.hotReloadVersion > 0)
                    {
                        const size_t extensionIndex = assemblyPath.FindLastIndex(".dll");

                        if (extensionIndex != ANSIString::NotFound)
                        {
                            assemblyPath = assemblyPath.Substr(0, extensionIndex)
                                + "." + ANSIString::ToString(scriptDesc.hotReloadVersion)
                                + ".dll";
                        }
                        else
                        {
                            assemblyPath = assemblyPath
                                + "." + ANSIString::ToString(scriptDesc.hotReloadVersion)
                                + ".dll";
                        }
                    }

                    if (SharedPtr<dotnet::Assembly> assembly = DotNETHost::GetInstance().LoadAssembly(assemblyPath.Data()))
                    {
                        scriptComponent.assembly = std::move(assembly);

                        // @TODO Set bytecode to be assembly binary data.
                    }
                    else
                    {
                        HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", assemblyPath.Data());

                        scriptAsset->SetBytecode(ConstByteView());

                        return;
                    }
                }

                if (SharedPtr<dotnet::ManagedClass> classPtr = scriptComponent.assembly->FindClassByName(scriptDesc.className.Data()))
                {
                    HYP_LOG(Scripting, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

                    if (!classPtr->HasParentClass("Script"))
                    {
                        HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

                        return;
                    }

                    dotnet::ManagedObject* object = classPtr->NewObject();
                    Assert(object != nullptr);

                    sor = new ScriptObjectResource(object, classPtr);
                    sor->AddReader();

                    if (!gameState.IsStopped())
                    {
                        if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED))
                        {
                            if (dotnet::ManagedMethod* beforeInitMethodPtr = classPtr->GetMethod("BeforeAdded"))
                            {
                                object->InvokeMethod<void>(beforeInitMethodPtr, world, scene);
                            }

                            if (dotnet::ManagedMethod* initMethodPtr = classPtr->GetMethod("OnAdded"))
                            {
                                object->InvokeMethod<void>(initMethodPtr, entity);
                            }

                            scriptComponent.flags |= ScriptComponentFlags::ACTIVATED;
                        }
                    }

                    HYP_LOG(Scripting, Verbose, "Created ScriptObjectResource for ScriptComponent, .NET class: {}", classPtr->GetName());
                }
#if HYP_DEBUG_MODE
                else
                {
                    HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptDesc.className.Data(), scriptComponent.assembly->GetGuid().ToUUID().ToString());
                }
#endif

                if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
                {
                    HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

                    if (scriptComponent.scriptObjectResource)
                    {
                        scriptComponent.scriptObjectResource->ReleaseReader();

                        delete scriptComponent.scriptObjectResource;
                        scriptComponent.scriptObjectResource = nullptr;
                    }

                    return;
                }
            }

            break;
        }
#endif // HYP_DOTNET
#ifdef HYP_SCRIPT
        case ScriptLanguage::HypScript:
        {
            namespace HS = HypScript;

            if (!sor || !sor->GetScriptObjectData_HypScript() || !sor->GetScriptObjectData_HypScript()->instance)
            {
                delete sor;
                sor = nullptr;

                auto readScope = scriptAsset->GetReadScope();

                ScriptInstance* instance = nullptr;

                // Create from bytecode
                ConstByteView bytecode = scriptAsset->GetBytecode();

#ifdef HYP_EDITOR
                if (bytecode.Size() > 0)
                {
                    // Check if source file has been modified since the bytecode was compiled
                    bool needsRecompile = false;

                    {
                        Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();

                        if (registry.IsValid())
                        {
                            const FilePath sourcePath = registry->GetRootPath() / "Scripts" / (scriptAsset->GetName().ToString() + ".hyp");

                            if (sourcePath.Exists() && sourcePath.CanRead())
                            {
                                const Time sourceModified = sourcePath.LastModifiedTimestamp();
                                const Time lastCompiled(scriptDesc.lastModifiedTimestamp);

                                if (sourceModified > lastCompiled)
                                {
                                    needsRecompile = true;
                                }
                            }
                        }
                    }

                    if (!needsRecompile)
                    {
                        instance = HS::CreateFromBytecode(bytecode);
                        Assert(instance != nullptr);
                    }
                }

                if (!instance) // needs recompile
                {
                    // Load source file and compile it
                    Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();
                    AssertDebug(registry.IsValid());

                    if (!registry.IsValid())
                    {
                        HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Invalid AssetRegistry, cannot load script source", scriptAsset->GetName());
                        return;
                    }

                    const FilePath sourcePath = registry->GetRootPath() / "Scripts" / (scriptAsset->GetName().ToString() + ".hyp");

                    if (!sourcePath.Exists() || !sourcePath.CanRead())
                    {
                        HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Script file '{}' does not exist or cannot be read!", scriptDesc.path.Data());
                        return;
                    }

                    EditorTaskScope editorTaskScope(
                        TickableEditorTask::StaticClass(),
                        "Compiling script",
                        "Processing source file: " + sourcePath,
                        /* isForegroundTask */ true);

                    FileByteReader readStream { sourcePath };

                    if (readStream.Eof())
                    {
                        HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Failed to open script file '{}' for reading!", scriptDesc.path.Data());
                        return;
                    }

                    ByteBuffer byteBuffer = readStream.Read();

                    SourceFile sourceFile(sourcePath, byteBuffer.Size());
                    sourceFile.ReadIntoBuffer(byteBuffer);

                    byteBuffer.Clear();

                    HypScriptCompileParams compileParams;
                    // Add data / scripts path as scan path so we pick up Lib.hyp
                    compileParams.scanPaths.Add(EngineGlobals::GetDataDirectory() / "Scripts");

                    ErrorList errorList;
                    instance = HS::Compile(sourceFile, errorList, compileParams);

                    if (errorList.HasFatalErrors())
                    {
                        SystemMessageBox(MessageBoxType::CRITICAL)
                            .Title("Script Compilation Error")
                            .Text(HYP_FORMAT("Failed to compile script file '{}'. See the log for details.", sourcePath))
                            .Button("Close", []()
                                    {
                                    })
                            .Show(/* showBlocking */ false);

                        return;
                    }

                    Assert(instance != nullptr);

#if 1
                    {
                        // Debug: decompile the bytecode
                        std::stringstream ss;
                        HS::Decompile(instance, &ss);
                        HYP_LOG(Scripting, Debug, "Decompiled bytecode:\n\n{}", ss.str().c_str());
                    }
#endif

                    // Record the source file timestamp so we can detect future changes
                    scriptDesc.lastModifiedTimestamp = uint64(sourcePath.LastModifiedTimestamp());

                    { // Save bytecode.
                        MemoryByteWriter<DynamicAllocator> bytecodeStream;
                        HS::WriteBytecodeToStream(instance, bytecodeStream);

                        readScope.Reset();

                        // Get exclusive access to write the bytecode data.
                        auto writeScope = scriptAsset->GetWriteScope();
                        scriptAsset->SetBytecode(bytecodeStream.GetBuffer().ToByteView());
                        writeScope.Reset();

                        // Save script binary again if it exists on the filesystem.
                        if (scriptAsset->IsSaved())
                        {
                            Result saveResult = scriptAsset->Save();
                            if (saveResult.HasError())
                            {
                                HYP_LOG(Scripting, Warning, "Failed to save script asset: {}", saveResult.GetError().GetMessage());
                            }
                        }

                        readScope = scriptAsset->GetReadScope();
                    }
                }
#else
                if (bytecode.Size() > 0)
                {
                    instance = HS::CreateFromBytecode(bytecode);
                    Assert(instance != nullptr);
                }
                else
                {
                    HYP_LOG(Scripting, Error, "Invalid bytecode for script: {}", scriptAsset->GetName());
                    return;
                }
#endif

                sor = new ScriptObjectResource(instance, (ObjectBase*)nullptr);
                sor->AddReader();

                if (!gameState.IsStopped())
                {

                    if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED))
                    {
                        // run the script to initialize classes, functions, etc.
                        HS::Run(instance);

                        InvokeScriptMethodT<void>(nullptr, sor, "BeforeAdded", world, scene);
                        InvokeScriptMethodT<void>(nullptr, sor, "OnAdded", entity);

                        scriptComponent.flags |= ScriptComponentFlags::ACTIVATED;
                    }
                }

                if (!sor || !sor->GetScriptObjectData_HypScript() || !sor->GetScriptObjectData_HypScript()->instance)
                {
                    if (scriptComponent.scriptObjectResource)
                    {
                        scriptComponent.scriptObjectResource->ReleaseReader();

                        delete scriptComponent.scriptObjectResource;
                        scriptComponent.scriptObjectResource = nullptr;
                    }

                    return;
                }
            }

            break;
        }
#endif // HYP_SCRIPT

#ifdef HYP_STRATA
        case ScriptLanguage::Strata:
        {
            if (!sor || !sor->GetScriptObjectData_Strata())
            {
                delete sor;
                sor = nullptr;

                ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

                const StringHash moduleHash(scriptDesc.path.Data());

                Strata::InitializeCache();
                Strata::t_fnPtrCache->ClearModule(moduleHash);

                sor = new ScriptObjectResource(ValueWrapper<ScriptLanguage::Strata>{}, moduleHash);
                sor->AddReader();

                if (ScriptObjectData_Strata* strataData = sor->GetScriptObjectData_Strata())
                {
                    strataData->moduleHash = moduleHash;

#ifdef HYP_STRATA_JIT
                    // Compile the source at runtime. Shipped builds have no knowledge of the language, symbols are linked to the exe
                    Strata::InitializeCompiler();

                    FilePath sourcePath;

                    if (Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry(); registry.IsValid())
                    {
                        sourcePath = registry->GetRootPath() / "Scripts" / (scriptAsset->GetName().ToString() + ".strata");
                    }

                    if (!sourcePath.Exists() || !sourcePath.CanRead())
                    {
                        // Fall back to the path recorded on the script descriptor.
                        sourcePath = FilePath(scriptDesc.path.Data());
                    }

                    if (sourcePath.Exists() && sourcePath.CanRead())
                    {
                        const char* err = nullptr;
                        StrataJit* jit = strataJitCompileFile(Strata::t_strataCompiler, sourcePath.Data(), &err);

                        if (jit != nullptr)
                        {
                            Strata::BindExterns(jit);

                            strataData->jit = jit;
                        }
                        else
                        {
                            HYP_LOG(Scripting, Error, "ScriptSystem::OnEntityAdded: Failed to JIT-compile Strata script '{}': {}",
                                sourcePath,
                                err ? err : "(no message)");

                            if (err != nullptr)
                            {
                                strataFree(const_cast<char*>(err));
                            }
                        }
                    }
                    else
                    {
                        HYP_LOG(Scripting, Warning, "Strata source '{}' not found; assuming AOT-linked symbols.",
                            scriptDesc.path.Data());
                    }
#endif // HYP_STRATA_JIT
                }

                if (!gameState.IsStopped())
                {
                    if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED))
                    {
                        InvokeScriptMethodT<void>(nullptr, sor, "BeforeAdded", world, scene);
                        InvokeScriptMethodT<void>(nullptr, sor, "OnAdded", entity);

                        scriptComponent.flags |= ScriptComponentFlags::ACTIVATED;
                    }
                }
            }

            break;
        }
#endif // HYP_STRATA
        default:
            return;
        }
    }

    scriptComponent.flags |= ScriptComponentFlags::INITIALIZED;
}

void ShutdownEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState)
{
    World* world = entity->GetWorld();

    if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    ScriptObjectResource*& sor = scriptComponent.scriptObjectResource;

    if (sor)
    {
        if (scriptComponent.flags & ScriptComponentFlags::ACTIVATED)
        {
            InvokeScriptMethod("Destroy", scriptComponent);
        }

        sor->ReleaseReader();

        delete sor;
        sor = nullptr;
    }

#ifdef HYP_SCRIPT
    HypScript::CollectGarbage();
#endif

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::ACTIVATED);
}

void UpdateScriptedEntities(World& world, float delta)
{
    QueryScriptedEntities(world, [delta](Entity* entity, ScriptComponent& scriptComponent)
                          {
                              if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED))
                                  return;

                              InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "Update", delta);
                          });
}

} // namespace EntityScripting

} // namespace Hyperion
