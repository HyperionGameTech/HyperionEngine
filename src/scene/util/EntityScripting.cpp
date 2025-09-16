/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/util/EntityScripting.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <asset/ScriptAsset.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

namespace hyperion {

extern FilePath CoreApi_GetExecutablePath();

#ifdef HYP_SCRIPT

extern Script_Value ScriptApi_MakeValue(HypData&& data);

#endif

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

        if (dotnet::Class* classPtr = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
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

        Script_Value functionValue;
        if (!hs.GetFunctionHandle(data->instance, methodName, functionValue))
        {
            break;
        }

        Script_Value returnValue = hs.CallFunction(data->instance, functionValue);

        if constexpr (!std::is_void_v<ReturnType>)
        {
            Assert(returnValue.IsValid());
            Assert(returnValue.GetHypData()->Is<ReturnType>());

            AssertDebug(outReturnValue != nullptr);

            new (outReturnValue) ReturnType(std::move(returnValue.GetHypData()->Get<ReturnType>()));
        }

        break;
    }
#endif
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

    ScriptData* scriptData = scriptComponent.scriptAsset->GetScriptData();
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

            Assert(scriptComponent.scriptAsset != nullptr);

            ResourceHandle resourceHandle(*scriptComponent.scriptAsset->GetResource());

            if (!scriptComponent.assembly)
            {
                ANSIString assemblyPath(scriptData->assemblyPath);

                if (scriptData->hotReloadVersion > 0)
                {
                    // @FIXME Implement FindLastIndex
                    const SizeType extensionIndex = assemblyPath.FindFirstIndex(".dll");

                    if (extensionIndex != ANSIString::notFound)
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
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", scriptData->assemblyPath);

                    return;
                }
            }

            if (RC<dotnet::Class> classPtr = scriptComponent.assembly->FindClassByName(scriptData->className))
            {
                HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptData->className, scriptData->assemblyPath);

                if (!classPtr->HasParentClass("Script"))
                {
                    HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptData->className, scriptData->assemblyPath);

                    return;
                }

                dotnet::Object* object = classPtr->NewObject();
                Assert(object != nullptr);

                sor = AllocateResource<ScriptObjectResource>(object, classPtr);
                sor->IncRef();

                HYP_LOG(Script, Debug, "Created ScriptObjectResource for ScriptComponent, .NET class: {}", classPtr->GetName());

                if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_INIT_CALLED))
                {
                    if (dotnet::Method* beforeInitMethodPtr = classPtr->GetMethod("BeforeInit"))
                    {
                        HYP_NAMED_SCOPE("Call BeforeInit() on script component");
                        HYP_LOG(Script, Debug, "Calling BeforeInit() on script component");

                        object->InvokeMethod<void>(beforeInitMethodPtr, world, scene);

                        scriptComponent.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
                    }
                }

                if (!(scriptComponent.flags & ScriptComponentFlags::INIT_CALLED))
                {
                    if (dotnet::Method* initMethodPtr = classPtr->GetMethod("Init"))
                    {
                        HYP_NAMED_SCOPE("Call Init() on script component");
                        HYP_LOG(Script, Info, "Calling Init() on script component");

                        object->InvokeMethod<void>(initMethodPtr, entity);

                        scriptComponent.flags |= ScriptComponentFlags::INIT_CALLED;
                    }
                }
            }
#ifdef HYP_DEBUG_MODE
            else
            {
                HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptData->className, scriptComponent.assembly->GetGuid().ToUUID().ToString());
            }
#endif

            if (!sor || !sor->GetManagedObject() || !sor->GetManagedObject()->IsValid())
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptData->className, scriptData->assemblyPath);

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

            Assert(scriptComponent.scriptAsset != nullptr);

            ResourceHandle resourceHandle(*scriptComponent.scriptAsset->GetResource());

            // @FIXME: Use proper path resolution. Should use asset system instead of filesystem directly.
            FilePath path = FilePath::Join(CoreApi_GetExecutablePath(), scriptData->path);

            if (!path.Exists() || !path.CanRead())
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Script file '{}' does not exist or cannot be read!", scriptData->path);
                return;
            }

            FileBufferedReaderSource source { path };
            BufferedByteReader reader { &source };

            if (!reader.IsOpen())
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to open script file '{}' for reading!", scriptData->path);
                return;
            }

            ByteBuffer byteBuffer = reader.ReadBytes();

            SourceFile sourceFile(path, byteBuffer.Size());
            sourceFile.ReadIntoBuffer(byteBuffer);

            ErrorList errorList;

            Script_Instance* instance = hs.Compile(sourceFile, errorList);

            if (errorList.HasFatalErrors())
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to compile script file '{}'!", scriptData->path);

                return;
            }

            Assert(instance != nullptr);

            InstructionStream* is = HypScript::GetInstance().Decompile(instance, &std::cout);
            delete is;

            // run the script to initialize classes, functions, etc.
            hs.Run(instance);

            sor = AllocateResource<ScriptObjectResource>(instance, Script_Value(), HYP_SCRIPT_TAG);
            sor->IncRef();

            if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_INIT_CALLED))
            {
                InvokeScriptMethod("BeforeInit", scriptComponent);
                scriptComponent.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
            }

            if (!(scriptComponent.flags & ScriptComponentFlags::INIT_CALLED))
            {
                InvokeScriptMethod("Init", scriptComponent);
                scriptComponent.flags |= ScriptComponentFlags::INIT_CALLED;
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

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
}

} // namespace hyperion
