/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <asset/ScriptAsset.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Method.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <system/MessageBox.hpp>

namespace hyperion {

extern FilePath CoreApi_GetExecutablePath();

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
{
    Assert(sor != nullptr);

    switch (sor->GetScriptLanguage())
    {
#ifdef HYP_DOTNET
    case SL_CSHARP:
    {
        AssertDebug(sor->GetManagedObject() != nullptr);

        if (dotnet::ManagedClass* classPtr = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::ManagedMethod* methodPtr = classPtr->GetMethod(methodName))
            {
                if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented

                    break;
                }

                if constexpr (!std::is_void_v<ReturnType>)
                {
                    AssertDebug(outReturnValue != nullptr);
                    new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(methodPtr, std::forward<ArgTypes>(args)...));
                }
                else
                {
                    sor->GetManagedObject()->InvokeMethod<void>(methodPtr, std::forward<ArgTypes>(args)...);
                }
            }
        }

        break;
    }
#endif
#ifdef HYP_SCRIPT
    case SL_HYPSCRIPT:
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        HypScript& hs = HypScript::GetInstance();

        HypData functionValue;
        if (!hs.GetFunctionHandle(data->instance, methodName, functionValue))
        {
            break;
        }

        HypData returnValue = hs.CallFunction(data->instance, functionValue);

        if constexpr (!std::is_void_v<ReturnType>)
        {
            Assert(returnValue.IsValid());
            Assert(returnValue.Is<ReturnType>());

            AssertDebug(outReturnValue != nullptr);

            new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
        }

        break;
    }
#endif
    case SL_NATIVE:
    {
        auto* data = sor->GetScriptObjectData_Native();
        Assert(data != nullptr);

        Handle<HypObjectBase> nativeObject = data->nativeObject.Lock();
        AssertDebug(nativeObject != nullptr);

        if (const Method* method = nativeObject->InstanceClass()->GetMethod(methodName))
        {
            if constexpr (!std::is_void_v<ReturnType>)
            {
                AssertDebug(outReturnValue != nullptr);
                new (outReturnValue) ReturnType(method->Invoke(Span<HypData> { { HypData(nativeObject), HypData(std::forward<ArgTypes>(args))... } }));
            }
            else
            {
                (void)method->Invoke(Span<HypData> { { HypData(nativeObject), HypData(std::forward<ArgTypes>(args))... } });
            }
        }

        break;
    }
    default:
        break;
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
            FreeResource<ScriptObjectResource>(sor);
            sor = nullptr;
        }

        sor = AllocateResource<ScriptObjectResource>(scriptComponent.nativeObject);
        sor->IncRef();

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

        ScriptData* scriptData = scriptAsset->GetScriptData();
        Assert(scriptData != nullptr);

        switch (scriptData->language)
        {
#ifdef HYP_DOTNET
        case SL_CSHARP:
        {
            if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
            {
                FreeResource<ScriptObjectResource>(sor);
                sor = nullptr;

                ResourceHandle resourceHandle(*scriptAsset->GetResource());

                if (!scriptComponent.assembly)
                {
                    ANSIString assemblyPath(scriptData->assemblyPath.Data(), scriptData->assemblyPath.Data() + ArraySize(scriptData->assemblyPath));

                    if (scriptData->hotReloadVersion > 0)
                    {
                        // @FIXME Implement FindLastIndex
                        const SizeType extensionIndex = assemblyPath.FindFirstIndex(".dll");

                        if (extensionIndex != ANSIString::NotFound)
                        {
                            assemblyPath = assemblyPath.Substr(0, extensionIndex)
                                + "." + ANSIString::ToString(scriptData->hotReloadVersion)
                                + ".dll";
                        }
                        else
                        {
                            assemblyPath = assemblyPath
                                + "." + ANSIString::ToString(scriptData->hotReloadVersion)
                                + ".dll";
                        }
                    }

                    if (RC<dotnet::Assembly> assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(assemblyPath.Data()))
                    {
                        scriptComponent.assembly = std::move(assembly);
                    }
                    else
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", assemblyPath.Data());

                        return;
                    }
                }

                if (RC<dotnet::ManagedClass> classPtr = scriptComponent.assembly->FindClassByName(scriptData->className.Data()))
                {
                    HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptData->className.Data(), scriptData->assemblyPath.Data());

                    if (!classPtr->HasParentClass("Script"))
                    {
                        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptData->className.Data(), scriptData->assemblyPath.Data());

                        return;
                    }

                    dotnet::ManagedObject* object = classPtr->NewObject();
                    Assert(object != nullptr);

                    sor = AllocateResource<ScriptObjectResource>(object, classPtr);
                    sor->IncRef();

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
                    HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptData->className.Data(), scriptComponent.assembly->GetGuid().ToUUID().ToString());
                }
#endif

                if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptData->className.Data(), scriptData->assemblyPath.Data());

                    if (scriptComponent.scriptObjectResource)
                    {
                        scriptComponent.scriptObjectResource->DecRef();

                        FreeResource<ScriptObjectResource>(scriptComponent.scriptObjectResource);
                        scriptComponent.scriptObjectResource = nullptr;
                    }

                    return;
                }
            }

            break;
        }
#endif
#ifdef HYP_SCRIPT
        case SL_HYPSCRIPT:
        {
            HypScript& hs = HypScript::GetInstance();

            if (!sor || !sor->GetScriptObjectData_HypScript() || !sor->GetScriptObjectData_HypScript()->instance)
            {
                FreeResource<ScriptObjectResource>(sor);
                sor = nullptr;

                ResourceHandle resourceHandle(*scriptAsset->GetResource());

                // @FIXME: Use proper path resolution. Should use asset system instead of filesystem directly.
                FilePath path = FilePath::Join(CoreApi_GetExecutablePath(), scriptData->path.Data());

                if (!path.Exists() || !path.CanRead())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Script file '{}' does not exist or cannot be read!", scriptData->path.Data());
                    return;
                }

                FileBufferedReaderSource source { path };
                BufferedByteReader reader { &source };

                if (!reader.IsOpen())
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to open script file '{}' for reading!", scriptData->path.Data());
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
                        .Text(HYP_FORMAT("Failed to compile script file '{}'. See the log for details.", scriptData->path.Data()))
                        .Button("Close", []()
                            {
                            })
                        .Show();

                    return;
                }

                Assert(instance != nullptr);

                // run the script to initialize classes, functions, etc.
                hs.Run(instance);

                sor = AllocateResource<ScriptObjectResource>(instance, HypData());
                sor->IncRef();

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
                        scriptComponent.scriptObjectResource->DecRef();

                        FreeResource<ScriptObjectResource>(scriptComponent.scriptObjectResource);
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

        sor->DecRef();

        FreeResource<ScriptObjectResource>(sor);
        sor = nullptr;
    }

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_ADDED_CALLED | ScriptComponentFlags::ON_ADDED_CALLED);
}

} // namespace hyperion
