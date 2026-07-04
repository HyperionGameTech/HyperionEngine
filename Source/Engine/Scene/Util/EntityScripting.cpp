/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Util/EntityScripting.hpp>

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

#include <Core/IO/BufferedByteReader.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Method.hpp>

#include <Framework/Game.hpp>

#include <Editor/EditorTask.hpp>

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <System/MessageBox.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Scripting);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

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
#endif

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
#endif
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
#if HYP_EDITOR
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
#endif
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
