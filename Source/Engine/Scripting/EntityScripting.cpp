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

#include <Core/IO/ByteReader.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

#ifdef HYP_EDITOR
#    include <Editor/EditorTask.hpp>
#endif // HYP_EDITOR


#ifdef HYP_STRATA
#    include <Core/Scripting/Strata/StrataMarshal.hpp>
#endif // HYP_STRATA

#ifdef HYP_STRATA_JIT
#    include <strata/strata.h>

#    include <Core/Scripting/Strata/ThunkDrawer.hpp>
#endif // HYP_STRATA_JIT

#if HYP_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#elif HYP_UNIX
#    include <dlfcn.h>
#endif

#include <System/MessageBox.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Scripting);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

#ifdef HYP_STRATA

namespace Strata {

extern "C"
{
    void* strata_alloc(size_t count)
    {
        return Alloc(count);
    }

    static void strata_free(void* ptr)
    {
        Free(ptr);
    }
} // extern "C"

// thread-local cache for strata module -> function pointer map
struct FunctionPointerCache
{
    using FunctionMap = Map<StringHash, void*>;
    using ModuleMap = Map<StringHash, FunctionMap>;

    ModuleMap modules;
    ModuleMap warnedMissing;

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

    bool ShouldWarnMissing(StringHash moduleHash, StringHash functionHash)
    {
        FunctionMap& fnMap = warnedMissing[moduleHash];

        if (fnMap.Find(functionHash) != fnMap.End())
        {
            return false;
        }

        fnMap[functionHash] = (void*)1;

        return true;
    }

    void ClearModule(StringHash moduleHash)
    {
        modules.Erase(moduleHash);
        warnedMissing.Erase(moduleHash);
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

#    if HYP_WINDOWS
    if (HMODULE h = GetModuleHandleW(nullptr))
    {
        return reinterpret_cast<void*>(GetProcAddress(h, name));
    }

    return nullptr;
#    elif HYP_UNIX
    // RTLD_DEFAULT searches the main program and all globally-loaded objects.
    return dlsym(RTLD_DEFAULT, name);
#    else
    return nullptr;
#    endif
}

void* ResolveFunctionPointer(ScriptObjectData_Strata* data, const char* name)
{
    Assert(data != nullptr);

    InitializeCache();

    const StringHash functionHash(name);

    if (void* cached = t_fnPtrCache->TryGet(data->moduleHash, functionHash))
    {
        return cached;
    }

    void* fn = nullptr;

#    ifdef HYP_STRATA_JIT
    if (data->jit != nullptr)
    {
        fn = strataJitGetFunction(data->jit, name);
    }
#    endif // HYP_STRATA_JIT

    if (fn == nullptr)
    {
        fn = ResolveSymbolFromHost(name);
    }

    if (fn == nullptr)
    {
        if (t_fnPtrCache->ShouldWarnMissing(data->moduleHash, functionHash))
        {
            HYP_LOG(Scripting, Warning, "Strata: function '{}' not found", name);
        }

        return nullptr;
    }

    t_fnPtrCache->Put(data->moduleHash, functionHash, fn);

    return fn;
}

#    ifdef HYP_STRATA_JIT

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

        strataJitSetAllocFreeFunctions(t_strataCompiler, (void*)&Strata::strata_alloc, (void*)&Strata::strata_free);

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

static FilePath CanonicalizeModulePath(const FilePath& path)
{
#        if HYP_WINDOWS
    char buffer[4096];

    if (_fullpath(buffer, path.Data(), sizeof(buffer)) != nullptr)
    {
        return FilePath(buffer);
    }
#        elif HYP_UNIX
    if (char* resolvedPath = realpath(path.Data(), nullptr))
    {
        FilePath result(resolvedPath);
        free(resolvedPath);

        return result;
    }
#        endif

    return path;
}

static const Array<FilePath>& GetEngineModuleDirectories()
{
    static const Array<FilePath> directories = []()
    {
        Array<FilePath> result;

#        ifdef HYP_ROOT_DIR
        // CodeGen writes Engine.strata into the source tree
        result.PushBack(FilePath(HYP_ROOT_DIR) / "Data" / "Scripts" / "Strata");
#        endif

        result.PushBack(CoreApi::GetExecutablePath() / "Data" / "Scripts" / "Strata");

        return result;
    }();

    return directories;
}

class ImportResolver
{
public:
    ImportResolver(StrataCompiler* compiler, const FilePath& projectScriptsDirectory)
        : m_compiler(compiler),
          m_projectScriptsDirectory(projectScriptsDirectory)
    {
        strataSetImportResolver(m_compiler, &ImportResolver::Resolve, this);
    }

    ImportResolver(const ImportResolver& other) = delete;
    ImportResolver& operator=(const ImportResolver& other) = delete;

    ~ImportResolver()
    {
        strataSetImportResolver(m_compiler, nullptr, nullptr);
    }

private:
    struct LoadedModule
    {
        FilePath path;
        ByteBuffer source;
    };

    static int Resolve(void* userData, const char* importerName, const char* importPath, StrataResolvedModule* out)
    {
        ImportResolver* resolver = static_cast<ImportResolver*>(userData);
        AssertDebug(resolver != nullptr);

        String moduleFilename(importPath);

        if (!moduleFilename.EndsWith(".strata"))
        {
            moduleFilename += ".strata";
        }

        // ./x and ../x only make sense relative to the importing module
        const bool isRelativeImport = importPath[0] == '.';

        Array<FilePath> searchDirectories;

        if (!isRelativeImport)
        {
            // Engine modules come first so a stale project copy of Engine.strata can't shadow the bindings this build exposes
            searchDirectories.Concat(GetEngineModuleDirectories());
        }

        searchDirectories.PushBack(FilePath(importerName).BasePath());

        if (!isRelativeImport)
        {
            searchDirectories.PushBack(resolver->m_projectScriptsDirectory);
        }

        for (const FilePath& directory : searchDirectories)
        {
            if (directory.Empty())
            {
                continue;
            }

            const FilePath candidatePath = directory / moduleFilename;

            if (!candidatePath.Exists() || candidatePath.IsDirectory() || !candidatePath.CanRead())
            {
                continue;
            }

            const LoadedModule& loadedModule = resolver->Load(CanonicalizeModulePath(candidatePath));

            out->text = loadedModule.source.Any() ? reinterpret_cast<const char*>(loadedModule.source.Data()) : "";
            out->length = loadedModule.source.Size();
            out->name = loadedModule.path.Data();

            return 1;
        }

        HYP_LOG(Scripting, Warning, "Strata: could not resolve import '{}' from '{}'", importPath, importerName);

        return 0;
    }

    const LoadedModule& Load(const FilePath& modulePath)
    {
        for (const LoadedModule& loadedModule : m_loadedModules)
        {
            if (loadedModule.path == modulePath)
            {
                return loadedModule;
            }
        }

        FileByteReader stream { modulePath };

        return m_loadedModules.PushBack(LoadedModule { modulePath, stream.Read() });
    }

    StrataCompiler* m_compiler;
    FilePath m_projectScriptsDirectory;
    Array<LoadedModule> m_loadedModules;
};

#    endif // HYP_STRATA_JIT

void ClearFunctionPointerCacheForModule(StringHash moduleHash)
{
    if (t_fnPtrCache != nullptr)
    {
        t_fnPtrCache->ClearModule(moduleHash);
    }
}

} // namespace Strata

#endif // HYP_STRATA

namespace EntityScripting {

template <class ReturnType, class... ArgTypes>
static bool InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, const ArgTypes&... args)
{
    Assert(sor != nullptr);

    const uint32 mask = sor->GetScriptLanguageMask();

    bool succeeded = true;

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

                        new (outReturnValue) ReturnType();

                        succeeded = sor->GetManagedObject()->TryInvokeMethod<ReturnType>(managedMethod, outReturnValue, args...);
                    }
                    else
                    {
                        succeeded = sor->GetManagedObject()->TryInvokeMethod<void>(managedMethod, nullptr, args...);
                    }
                }
            }
        }
    }
#endif // HYP_DOTNET

#ifdef HYP_STRATA
    if (mask & (1u << uint32(ScriptLanguage::Strata)))
    {
        auto* data = sor->GetScriptObjectData_Strata();
        Assert(data != nullptr);

        if (void* fnPtrRaw = Strata::ResolveFunctionPointer(data, methodName))
        {
            if (data->context != nullptr)
            {
                auto fnPtrCasted = (ReturnType (*)(void*, ArgTypes...))fnPtrRaw;

                if constexpr (!std::is_void_v<ReturnType>)
                {
                    AssertDebug(outReturnValue != nullptr);

                    new (outReturnValue) ReturnType(fnPtrCasted(data->context, args...));
                }
                else
                {
                    fnPtrCasted(data->context, args...);
                }
            }
            else
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

    return succeeded;
}

static HYP_FORCE_INLINE bool InvokeScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    return InvokeScriptMethodT<void>(nullptr, target.scriptObjectResource, *methodName);
}

static void MarkEntityScriptErrored(Entity* entity, ScriptComponent& scriptComponent, const char* methodName)
{
    HYP_LOG(Scripting, Error, "Script on entity {} threw an exception in {}", entity->Id(), methodName);

    scriptComponent.flags |= ScriptComponentFlags::ERRORED;
}

static void ActivateEntityScript(Entity* entity, ScriptComponent& scriptComponent, World* world, Scene* scene)
{
    if (scriptComponent.flags & ScriptComponentFlags::ACTIVATED)
    {
        return;
    }

    if (!InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "BeforeAdded", world, scene))
    {
        MarkEntityScriptErrored(entity, scriptComponent, "BeforeAdded");
    }
    else if (!InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "OnAdded", entity))
    {
        MarkEntityScriptErrored(entity, scriptComponent, "OnAdded");
    }

    scriptComponent.flags |= ScriptComponentFlags::ACTIVATED;
}

ANSIString GetCSharpAssemblyLoadPath(const ScriptDesc& scriptDesc)
{
    ANSIString assemblyPath(scriptDesc.assemblyPath.Data(), scriptDesc.assemblyPath.Data() + scriptDesc.assemblyPath.Size());

    if (scriptDesc.hotReloadVersion <= 0)
    {
        return assemblyPath;
    }

    const size_t extensionIndex = assemblyPath.FindLastIndex(".dll");

    if (extensionIndex != ANSIString::NotFound)
    {
        return assemblyPath.Substr(0, extensionIndex)
            + "." + ANSIString::ToString(scriptDesc.hotReloadVersion)
            + ".dll";
    }

    return assemblyPath
        + "." + ANSIString::ToString(scriptDesc.hotReloadVersion)
        + ".dll";
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
            ActivateEntityScript(entity, scriptComponent, world, scene);
        }
    }
    else // external script object (C# or Strata)
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
                    const ANSIString assemblyPath = GetCSharpAssemblyLoadPath(scriptDesc);

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

                    if (dotnet::ManagedObject* object = classPtr->NewObject())
                    {
                        sor = new ScriptObjectResource(object, classPtr);
                        sor->AddReader();

                        if (!gameState.IsStopped())
                        {
                            ActivateEntityScript(entity, scriptComponent, world, scene);
                        }

                        HYP_LOG(Scripting, Verbose, "Created ScriptObjectResource for ScriptComponent, .NET class: {}", classPtr->GetName());
                    }
                }
#    if HYP_DEBUG_MODE
                else
                {
                    HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptDesc.className.Data(), scriptComponent.assembly->GetGuid().ToUUID().ToString());
                }
#    endif

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

                sor = new ScriptObjectResource(ValueWrapper<ScriptLanguage::Strata> {}, moduleHash);
                sor->AddReader();

                if (ScriptObjectData_Strata* strataData = sor->GetScriptObjectData_Strata())
                {
                    strataData->moduleHash = moduleHash;

#    ifdef HYP_STRATA_JIT
                    // Compile the source at runtime. Shipped builds have no knowledge of the language, symbols are linked to the exe
                    Strata::InitializeCompiler();

                    FilePath projectScriptsDirectory;
                    FilePath sourcePath;

                    if (Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry(); registry.IsValid())
                    {
                        projectScriptsDirectory = registry->GetRootPath() / AssetBuckets::Scripts.GetName();
                        sourcePath = projectScriptsDirectory / (scriptAsset->GetName().ToString() + ".strata");
                    }

                    if (!sourcePath.Exists() || !sourcePath.CanRead())
                    {
                        // Fall back to the path recorded on the script descriptor.
                        sourcePath = FilePath(scriptDesc.path.Data());
                    }

                    if (sourcePath.Exists() && sourcePath.CanRead())
                    {
                        Strata::ImportResolver importResolver(Strata::t_strataCompiler, projectScriptsDirectory);

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
#    else
                    HYP_LOG(Scripting, Warning, "This build has no LLVM JIT backend, so '{}' will only run "
                                                 "if it was AOT-compiled with stratac. Scripts created or edited after the build will not execute!",
                            scriptAsset->GetName());
#    endif // HYP_STRATA_JIT
                    if (void* createFnRaw = Strata::ResolveFunctionPointer(strataData, "__strata_context_create"))
                    {
                        auto createFn = (void* (*)(void))createFnRaw;
                        strataData->context = createFn();
                    }
                }

                if (!gameState.IsStopped())
                {
                    ActivateEntityScript(entity, scriptComponent, world, scene);
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


    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::ACTIVATED | ScriptComponentFlags::ERRORED);
}

void UpdateScriptedEntities(World& world, float delta)
{
    QueryScriptedEntities(world, [delta](Entity* entity, ScriptComponent& scriptComponent)
                          {
                              if (!(scriptComponent.flags & ScriptComponentFlags::ACTIVATED) || (scriptComponent.flags & ScriptComponentFlags::ERRORED))
                                  return;

                              if (!InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "Update", delta))
                              {
                                  MarkEntityScriptErrored(entity, scriptComponent, "Update");
                              }
                          });
}

} // namespace EntityScripting

} // namespace Hyperion
