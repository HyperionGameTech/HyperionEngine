/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNETHost.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Method.hpp>

#include <engine/Game.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <system/MessageBox.hpp>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

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

        HYP_LOG(Script, Debug, "Created ScriptObjectResource for ScriptComponent, native class: {}", nativeClass->GetName());

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
        const Handle<ScriptAsset>& scriptAsset = scriptComponent.assetReference.Resolve();
        AssertDebug(scriptAsset != nullptr);

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

                auto resGuard = scriptAsset->GetReadScope();

                if (!scriptComponent.assembly)
                {
                    ANSIString assemblyPath(scriptDesc.assemblyPath.Data(), scriptDesc.assemblyPath.Data() + ArraySize(scriptDesc.assemblyPath));

                    if (scriptDesc.hotReloadVersion > 0)
                    {
                        // @FIXME Implement FindLastIndex
                        const SizeType extensionIndex = assemblyPath.FindFirstIndex(".dll");

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
                    }
                    else
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", assemblyPath.Data());

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

                    HYP_LOG(Script, Debug, "Created ScriptObjectResource for ScriptComponent, .NET class: {}", classPtr->GetName());

                    if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_ADDED_CALLED))
                    {
                        if (dotnet::ManagedMethod* beforeInitMethodPtr = classPtr->GetMethod("BeforeAdded"))
                        {
                            HYP_NAMED_SCOPE("Call BeforeAdded() on script component");
                            HYP_LOG(Script, Debug, "Calling BeforeAdded() on script component");

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
#ifdef HYP_DEBUG_MODE
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

                auto resGuard = scriptAsset->GetReadScope();

                // @FIXME: Use proper path resolution. Should use asset system instead of filesystem directly.
                FilePath path = FilePath::Join(CoreApi::GetExecutablePath(), scriptDesc.path.Data());

                if (!path.Exists() || !path.CanRead())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Script file '{}' does not exist or cannot be read!", scriptDesc.path.Data());
                    return;
                }

                FileBufferedReaderSource source { path };
                BufferedByteReader reader { &source };

                if (!reader.IsOpen())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to open script file '{}' for reading!", scriptDesc.path.Data());
                    return;
                }

                ByteBuffer byteBuffer = reader.ReadBytes();

                SourceFile sourceFile(path, byteBuffer.Size());
                sourceFile.ReadIntoBuffer(byteBuffer);

                ErrorList errorList;

                Script_Instance* instance = hs.Compile(sourceFile, errorList);

                if (errorList.HasFatalErrors())
                {
                    SystemMessageBox(MessageBoxType::CRITICAL)
                        .Title("Script Compilation Error")
                        .Text(HYP_FORMAT("Failed to compile script file '{}'. See the log for details.", scriptDesc.path.Data()))
                        .Button("Close", []()
                            {
                            })
                        .Show(/* showBlocking */ false);

                    return;
                }

                Assert(instance != nullptr);

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
        default:
            return;
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
