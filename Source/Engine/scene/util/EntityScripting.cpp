/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetRegistry.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNETHost.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Method.hpp>

#include <engine/Game.hpp>

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <system/MessageBox.hpp>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

extern const FilePath& GetDataDirectory();

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
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
                        new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(managedMethod, std::forward<ArgTypes>(args)...));
                    }
                    else
                    {
                        sor->GetManagedObject()->InvokeMethod<void>(managedMethod, std::forward<ArgTypes>(args)...);
                    }
                }
            }
        }
    }
#endif

#ifdef HYP_SCRIPT
    if (mask & (1u << uint32(ScriptLanguage::HypScript)))
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        HypScript& hs = HypScript::GetInstance();

        BoxedValue functionValue;
        if (hs.GetFunctionHandle(data->instance, methodName, functionValue))
        {
            BoxedValue returnValue = hs.CallFunction(data->instance, functionValue);

            if constexpr (!std::is_void_v<ReturnType>)
            {
                Assert(returnValue.IsValid());
                Assert(returnValue.Is<ReturnType>());

                AssertDebug(outReturnValue != nullptr);

                new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
            }
        }
    }
#endif

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
                new (outReturnValue) ReturnType(method->Invoke(Span<BoxedValue> { { BoxedValue(nativeObject), BoxedValue(std::forward<ArgTypes>(args))... } }));
            }
            else
            {
                (void)method->Invoke(Span<BoxedValue> { { BoxedValue(nativeObject), BoxedValue(std::forward<ArgTypes>(args))... } });
            }
        }
    }
}

static HYP_FORCE_INLINE void InvokeScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    InvokeScriptMethodT<void>(nullptr, target.scriptObjectResource, *methodName);
}

void EntityScripting::InitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent)
{
    World* world = entity->GetWorld();
    Scene* scene = entity->GetScene();

    ScriptObjectResource*& sor = scriptComponent.scriptObjectResource;

    if (scriptComponent.flags & ScriptComponentFlags::INITIALIZED)
    {
        AssertDebug(sor != nullptr);

        if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
        {
            InvokeScriptMethod("OnPlayStart", scriptComponent);
        }

        return;
    }

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

        HYP_LOG(Script, Verbose, "Created ScriptObjectResource for ScriptComponent, native class: {}", nativeClass->GetName());

        InitObject(scriptComponent.nativeObject);

        if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_ADDED_CALLED))
        {
            InvokeScriptMethodT<void>(nullptr, sor, "BeforeAdded", world, scene);

            scriptComponent.flags |= ScriptComponentFlags::BEFORE_ADDED_CALLED;
        }

        if (!(scriptComponent.flags & ScriptComponentFlags::ON_ADDED_CALLED))
        {
            InvokeScriptMethodT<void>(nullptr, sor, "OnAdded", entity);

            scriptComponent.flags |= ScriptComponentFlags::ON_ADDED_CALLED;
        }
    }
    else // external script object (C# or HypScript)
    {
        const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;

        if (!scriptAsset)
        {
            HYP_LOG(Script, Warning, "Entity has ScriptComponent with no ScriptAsset!");

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
                        // @FIXME Implement FindLastIndex
                        const size_t extensionIndex = assemblyPath.FindFirstIndex(".dll");

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

                    if (RC<dotnet::Assembly> assembly = DotNETHost::GetInstance().LoadAssembly(assemblyPath.Data()))
                    {
                        scriptComponent.assembly = std::move(assembly);

                        // @TODO Set bytecode to be assembly binary data.
                    }
                    else
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", assemblyPath.Data());

                        scriptAsset->SetBytecode(ConstByteView());

                        return;
                    }
                }

                if (RC<dotnet::ManagedClass> classPtr = scriptComponent.assembly->FindClassByName(scriptDesc.className.Data()))
                {
                    HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

                    if (!classPtr->HasParentClass("Script"))
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

                        return;
                    }

                    dotnet::ManagedObject* object = classPtr->NewObject();
                    Assert(object != nullptr);

                    sor = new ScriptObjectResource(object, classPtr);
                    sor->AddReader();

                    HYP_LOG(Script, Verbose, "Created ScriptObjectResource for ScriptComponent, .NET class: {}", classPtr->GetName());

                    if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_ADDED_CALLED))
                    {
                        if (dotnet::ManagedMethod* beforeInitMethodPtr = classPtr->GetMethod("BeforeAdded"))
                        {
                            HYP_NAMED_SCOPE("Call BeforeAdded() on script component");
                            HYP_LOG(Script, Verbose, "Calling BeforeAdded() on script component");

                            object->InvokeMethod<void>(beforeInitMethodPtr, world, scene);

                            scriptComponent.flags |= ScriptComponentFlags::BEFORE_ADDED_CALLED;
                        }
                    }

                    if (!(scriptComponent.flags & ScriptComponentFlags::ON_ADDED_CALLED))
                    {
                        if (dotnet::ManagedMethod* initMethodPtr = classPtr->GetMethod("OnAdded"))
                        {
                            HYP_NAMED_SCOPE("Call Init() on script component");
                            HYP_LOG(Script, Info, "Calling Init() on script component");

                            object->InvokeMethod<void>(initMethodPtr, entity);

                            scriptComponent.flags |= ScriptComponentFlags::ON_ADDED_CALLED;
                        }
                    }
                }
#if HYP_DEBUG_MODE
                else
                {
                    HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptDesc.className.Data(), scriptComponent.assembly->GetGuid().ToUUID().ToString());
                }
#endif

                if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptDesc.className.Data(), scriptDesc.assemblyPath.Data());

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
#endif
#ifdef HYP_SCRIPT
        case ScriptLanguage::HypScript:
        {
            HypScript& hs = HypScript::GetInstance();

            if (!sor || !sor->GetScriptObjectData_HypScript() || !sor->GetScriptObjectData_HypScript()->instance)
            {
                delete sor;
                sor = nullptr;

                auto readScope = scriptAsset->GetReadScope();

                ScriptInstance* instance = nullptr;

                // Create from bytecode
                ConstByteView bytecode = scriptAsset->GetBytecode();
#if HYP_EDITOR
                static const auto GetSourcePath = [](const AssetRegistry& registry, const ScriptAsset& scriptAsset) -> FilePath
                {
                    const FilePath sourceDir = registry.GetRootPath() / "Scripts";
                    FilePath sourcePath = sourceDir / (scriptAsset.GetName().ToString() + ".hyp");

                    if (Memory::StrLen(scriptAsset.GetScriptDesc().path.Data()) != 0)
                    {
                        sourcePath = (sourceDir / FilePath(scriptAsset.GetScriptDesc().path.Data())).Canonicalize();
                    }

                    return sourcePath;
                };

                static const auto GetRelativeSourcePath = [](const AssetRegistry& registry, const FilePath& sourcePath) -> String
                {
                    const FilePath sourceDir = registry.GetRootPath() / "Scripts";
                    return FilePath::Relative(sourcePath, sourceDir);
                };

                if (bytecode.Size() > 0)
                {
                    // Check if source file has been modified since the bytecode was compiled
                    bool needsRecompile = false;

                    {
                        Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();

                        if (registry.IsValid())
                        {
                            const FilePath sourcePath = GetSourcePath(*registry, *scriptAsset);
                            const String relativeSourcePath = GetRelativeSourcePath(*registry, sourcePath);

                            if (Memory::StrCmp(relativeSourcePath.Data(), scriptDesc.path.Data(), scriptDesc.path.Size()) != 0)
                            {
                                // needs recompile if source path differs
                                needsRecompile = true;
                            }
                            else if (sourcePath.Exists() && sourcePath.CanRead())
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
                        instance = hs.CreateFromBytecode(bytecode);
                        Assert(instance != nullptr);
                    }
                }

                if (!instance)
                {
                    // Load source file and compile it
                    Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();
                    AssertDebug(registry.IsValid());

                    if (!registry.IsValid())
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Invalid AssetRegistry, cannot load script source", scriptAsset->GetName());
                        return;
                    }

                    FilePath sourcePath = GetSourcePath(*registry, *scriptAsset);

                    if (!sourcePath.Exists() || !sourcePath.CanRead())
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Script file '{}' does not exist or cannot be read!", scriptDesc.path.Data());
                        return;
                    }

                    FileByteReader readStream { sourcePath };

                    if (readStream.Eof())
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to open script file '{}' for reading!", scriptDesc.path.Data());
                        return;
                    }

                    ByteBuffer byteBuffer = readStream.Read();
                    SourceFile sourceFile(sourcePath, byteBuffer.Size());
                    sourceFile.ReadIntoBuffer(byteBuffer);
                    byteBuffer.Clear();

                    // Get exclusive access to write the bytecode data and set state.
                    readScope.Reset();
                    auto writeScope = scriptAsset->GetWriteScope();

                    // Update status
                    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();
                    scriptDesc.compileStatus &= ~(ScriptCompileStatus::Compiled | ScriptCompileStatus::Errored);
                    scriptDesc.compileStatus |= ScriptCompileStatus::Processing;

                    HypScriptCompileParams compileParams;
                    // Add data / scripts path as scan path so we pick up Lib.hyp
                    compileParams.scanPaths.Add(GetDataDirectory() / "Scripts");
                    
                    ErrorList errorList;
                    instance = hs.Compile(sourceFile, errorList, compileParams);

                    if (errorList.HasFatalErrors())
                    {
                        scriptDesc.compileStatus &= ~ScriptCompileStatus::Processing;
                        scriptDesc.compileStatus |= ScriptCompileStatus::Errored;

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

                    // Record the source file timestamp so we can detect future changes
                    scriptDesc.lastModifiedTimestamp = uint64(sourcePath.LastModifiedTimestamp());

                    // Write filepath to desc
                    const FilePath sourceDir = registry->GetRootPath() / "Scripts";
                    const String relativeSourcePath = GetRelativeSourcePath(*registry, sourcePath);
                    Memory::StrCpy(scriptDesc.path.Data(), relativeSourcePath.Data(), scriptDesc.path.Size());

                    { // Save bytecode.
                        MemoryByteWriter bytecodeStream;
                        hs.WriteBytecodeToStream(instance, bytecodeStream);

                        scriptAsset->SetBytecode(bytecodeStream.GetBuffer().ToByteView());
                        
                        // Mark compiled
                        scriptDesc.compileStatus &= ~ScriptCompileStatus::Processing;
                        scriptDesc.compileStatus |= ScriptCompileStatus::Compiled;

                        writeScope.Reset();
                        
                        // Save script binary again if it exists on the filesystem.
                        if (scriptAsset->IsSaved())
                        {
                            Result saveResult = scriptAsset->Save();
                            if (saveResult.HasError())
                            {
                                HYP_LOG(Script, Warning, "Failed to save script asset: {}", saveResult.GetError().GetMessage());
                            }
                        }

                        readScope = scriptAsset->GetReadScope();
                    }
                }
#else
                if (bytecode.Size() > 0)
                {
                    instance = hs.CreateFromBytecode(bytecode);
                    Assert(instance != nullptr);
                }
                else
                {
                    HYP_LOG(Script, Error, "Invalid bytecode for script: {}", scriptAsset->GetName());
                    return;
                }
#endif

                // run the script to initialize classes, functions, etc.
                hs.Run(instance);

                sor = new ScriptObjectResource(instance, BoxedValue());
                sor->AddReader();

                if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_ADDED_CALLED))
                {
                    InvokeScriptMethodT<void>(nullptr, sor, "BeforeAdded", world, scene);
                    scriptComponent.flags |= ScriptComponentFlags::BEFORE_ADDED_CALLED;
                }

                if (!(scriptComponent.flags & ScriptComponentFlags::ON_ADDED_CALLED))
                {
                    InvokeScriptMethodT<void>(nullptr, sor, "OnAdded", entity);
                    scriptComponent.flags |= ScriptComponentFlags::ON_ADDED_CALLED;
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
#endif
        default:  return;
        }
    }

    scriptComponent.flags |= ScriptComponentFlags::INITIALIZED;

    // Call OnPlayStart on first init if we're currently simulating
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        InvokeScriptMethod("OnPlayStart", scriptComponent);
    }
}

void EntityScripting::DeinitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent)
{
    World* world = entity->GetWorld();

    if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    // If we're simulating while the script is removed, call OnPlayStop so OnPlayStart never gets double called
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        InvokeScriptMethod("OnPlayStop", scriptComponent);
    }

    ScriptObjectResource*& sor = scriptComponent.scriptObjectResource;

    if (sor)
    {
        InvokeScriptMethod("Destroy", scriptComponent);

        sor->ReleaseReader();

        delete sor;
        sor = nullptr;
    }

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_ADDED_CALLED | ScriptComponentFlags::ON_ADDED_CALLED);
}

} // namespace Hyperion
