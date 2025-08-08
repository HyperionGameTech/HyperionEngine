/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/util/EntityScripting.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

static void CallScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    AssertDebug(target.resource != nullptr);
    AssertDebug(target.resource->GetManagedObject() != nullptr);

    if (dotnet::Class* classPtr = target.resource->GetManagedObject()->GetClass())
    {
        if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
        {
            if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
            {
                // Stubbed method, don't waste cycles calling it if it's not implemented
                return;
            }

            target.resource->GetManagedObject()->InvokeMethod<void>(methodPtr);
        }
    }
}

void EntityScripting::InitEntityScriptComponent(Entity* entity, ScriptComponent& scriptComponent)
{
    World* world = entity->GetWorld();
    Scene* scene = entity->GetScene();

    if (scriptComponent.flags & ScriptComponentFlags::INITIALIZED)
    {
        AssertDebug(scriptComponent.resource != nullptr);
        if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
        {
            CallScriptMethod("OnPlayStart", scriptComponent);
        }

        return;
    }

    if (!scriptComponent.resource || !scriptComponent.resource->GetManagedObject() || !scriptComponent.resource->GetManagedObject()->IsValid())
    {
        FreeResource<ManagedObjectResource>(scriptComponent.resource);
        scriptComponent.resource = nullptr;

        if (!scriptComponent.assembly)
        {
            ANSIString assemblyPath(scriptComponent.script.assemblyPath);

            if (scriptComponent.script.hotReloadVersion > 0)
            {
                // @FIXME Implement FindLastIndex
                const SizeType extensionIndex = assemblyPath.FindFirstIndex(".dll");

                if (extensionIndex != ANSIString::notFound)
                {
                    assemblyPath = assemblyPath.Substr(0, extensionIndex)
                        + "." + ANSIString::ToString(scriptComponent.script.hotReloadVersion)
                        + ".dll";
                }
                else
                {
                    assemblyPath = assemblyPath
                        + "." + ANSIString::ToString(scriptComponent.script.hotReloadVersion)
                        + ".dll";
                }
            }

            if (RC<dotnet::Assembly> assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(assemblyPath.Data()))
            {
                scriptComponent.assembly = std::move(assembly);
            }
            else
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", scriptComponent.script.assemblyPath);

                return;
            }
        }

        if (RC<dotnet::Class> classPtr = scriptComponent.assembly->FindClassByName(scriptComponent.script.className))
        {
            HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

            if (!classPtr->HasParentClass("Script"))
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

                return;
            }

            dotnet::Object* object = classPtr->NewObject();
            Assert(object != nullptr);

            scriptComponent.resource = AllocateResource<ManagedObjectResource>(object, classPtr);
            scriptComponent.resource->IncRef();

            HYP_LOG(Script, Debug, "Created ManagedScriptResource for ScriptComponent, .NET class: {}", classPtr->GetName());

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
            HYP_FAIL("Failed to load .NET class {} from Assembly {}", scriptComponent.script.className, scriptComponent.assembly->GetGuid().ToUUID().ToString());
        }
#endif

        if (!scriptComponent.resource || !scriptComponent.resource->GetManagedObject() || !scriptComponent.resource->GetManagedObject()->IsValid())
        {
            HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

            if (scriptComponent.resource)
            {
                scriptComponent.resource->DecRef();

                FreeResource<ManagedObjectResource>(scriptComponent.resource);
                scriptComponent.resource = nullptr;
            }

            return;
        }
    }

    scriptComponent.flags |= ScriptComponentFlags::INITIALIZED;

    // Call OnPlayStart on first init if we're currently simulating
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart", scriptComponent);
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
        CallScriptMethod("OnPlayStop", scriptComponent);
    }

    if (scriptComponent.resource)
    {
        if (scriptComponent.resource->GetManagedObject() && scriptComponent.resource->GetManagedObject()->IsValid())
        {
            if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
            {
                if (classPtr->HasMethod("Destroy"))
                {
                    HYP_NAMED_SCOPE("Call Destroy() on script component");

                    scriptComponent.resource->GetManagedObject()->InvokeMethodByName<void>("Destroy");
                }
            }
        }

        scriptComponent.resource->DecRef();

        FreeResource<ManagedObjectResource>(scriptComponent.resource);
        scriptComponent.resource = nullptr;
    }

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
}

} // namespace hyperion
