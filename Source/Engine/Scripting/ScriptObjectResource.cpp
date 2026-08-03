#include <HyperionPch.hpp>

#include <Scripting/ScriptObjectResource.hpp>
#include <Scripting/Script.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Object.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Reflection/ScriptObjectFunctions.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/DotNETHost.hpp>
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif // HYP_SCRIPT

#ifdef HYP_STRATA_JIT
#include <strata/strata.h>
#endif // HYP_STRATA_JIT

#include <Framework/EngineStats.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

static EngineStatTimer s_statInitScriptResource("Script/InitScriptResource");

#pragma region ScriptObjectResource

ScriptObjectResource::ScriptObjectResource() = default;

ScriptObjectResource::ScriptObjectResource(const Handle<ObjectBase>& nativeObject)
    : m_ptr(nullptr)
{
    if (!nativeData)
    {
        nativeData.Emplace(ScriptObjectData_Native());
    }

    ScriptObjectData_Native& data = *nativeData;
    data.nativeObject = nativeObject;
}

#ifdef HYP_DOTNET
ScriptObjectResource::ScriptObjectResource(dotnet::ManagedObject* objectPtr, const SharedPtr<dotnet::ManagedClass>& managedClass)
    : m_ptr(nullptr)
{
    if (!dotNetData)
    {
        dotNetData.Emplace(ScriptObjectData_DotNet());
    }

    ScriptObjectData_DotNet& data = *dotNetData;
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;

    AssertDebug(data.objectPtr && data.managedClass);
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, const SharedPtr<dotnet::ManagedClass>& managedClass)
    : ScriptObjectResource(ptr, managedClass, {}, ObjectFlags::NONE)
{
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, dotnet::ManagedObject* objectPtr, const SharedPtr<dotnet::ManagedClass>& managedClass)
    : m_ptr(ptr)
{
    ScriptObjectData_DotNet& data = dotNetData.Emplace(ScriptObjectData_DotNet());
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;

    AssertDebug(data.objectPtr && data.managedClass);
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, const SharedPtr<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_ptr(ptr)
{
    const DotNETHost& dnh = DotNETHost::GetInstance();

    if (dnh.IsInitialized() && !dnh.IsShuttingDown())
    {
        ScriptObjectData_DotNet& data = dotNetData.Emplace(ScriptObjectData_DotNet());
        data.objectPtr = nullptr;
        data.managedClass = managedClass;

        AssertDebug(m_ptr && managedClass);
    
        if (m_ptr && managedClass)
        {
            if (objectFlags & ObjectFlags::CREATED_FROM_MANAGED)
            {
                data.objectPtr = new dotnet::ManagedObject(managedClass->SharedThis(), objectReference, ObjectFlags::CREATED_FROM_MANAGED);
            }
            else
            {
                data.objectPtr = managedClass->NewObject(m_ptr->InstanceClass(), m_ptr);
            }

            Assert(data.objectPtr != nullptr);
        }
    }
}
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT

ScriptObjectResource::ScriptObjectResource(ScriptInstance* hypScriptInstance, ObjectBase* hypScriptValue)
    : m_ptr(nullptr)
{
    ScriptObjectData_HypScript& data = hypScriptData.Emplace(ScriptObjectData_HypScript());
    data.instance = hypScriptInstance;
    data.obj = hypScriptValue;
}

#endif // HYP_SCRIPT

#ifdef HYP_STRATA
ScriptObjectResource::ScriptObjectResource(ValueWrapper<ScriptLanguage::Strata>, StringHash moduleHash)
    : m_ptr(nullptr)
{
    ScriptObjectData_Strata& data = strataData.Emplace(ScriptObjectData_Strata());
    data.moduleHash = moduleHash;
}
#endif // HYP_STRATA

ScriptObjectResource::~ScriptObjectResource()
{
#ifdef HYP_SCRIPT
    if (hypScriptData.HasValue())
    {
        if (hypScriptData->instance)
        {

            HypScript::DestroyScript(hypScriptData->instance);
            hypScriptData->instance = nullptr;
        }

        hypScriptData->obj = nullptr;

        hypScriptData.Unset();
    }
#endif // HYP_SCRIPT

#ifdef HYP_DOTNET
    if (dotNetData.HasValue())
    {
        if (dotNetData->objectPtr)
        {
            const DotNETHost& dnh = DotNETHost::GetInstance();

            if (dnh.IsInitialized() && !dnh.IsShuttingDown())
            {
                const bool result = dotNetData->objectPtr->SetKeepAlive(false);
                Assert(result);
            }

           delete dotNetData->objectPtr;
           dotNetData->objectPtr = nullptr;
        }

        dotNetData.Unset();
    }
#endif // HYP_DOTNET

#ifdef HYP_STRATA
    if (strataData.HasValue())
    {
#ifdef HYP_STRATA_JIT
        if (strataData->jit)
        {
            strataJitDestroy(strataData->jit);
            strataData->jit = nullptr;
        }
#endif // HYP_STRATA_JIT

        strataData.Unset();
    }
#endif // HYP_STRATA
}

uint32 ScriptObjectResource::GetScriptLanguageMask() const
{
    uint32 mask = 0;

    if (nativeData.HasValue())
    {
        mask |= (1 << uint32(ScriptLanguage::Native));
    }

#ifdef HYP_DOTNET
    if (dotNetData.HasValue())
    {
        mask |= (1 << uint32(ScriptLanguage::CSharp));
    }
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
    if (hypScriptData.HasValue())
    {
        mask |= (1 << uint32(ScriptLanguage::HypScript));
    }
#endif // HYP_SCRIPT
    
#ifdef HYP_STRATA
    if (strataData.HasValue())
    {
        mask |= (1 << uint32(ScriptLanguage::Strata));
    }
#endif // HYP_STRATA

    return mask;
}

dotnet::ManagedObject* ScriptObjectResource::GetManagedObject() const
{
#ifdef HYP_DOTNET
    // only valid to call on .NET script objects
    if (dotNetData.HasValue())
    {
        return dotNetData->objectPtr;
    }
#endif // HYP_DOTNET

    return nullptr;
}

const SharedPtr<dotnet::ManagedClass> ScriptObjectResource::GetManagedClass() const
{
#ifdef HYP_DOTNET
    // only valid to call on .NET script objects
    if (dotNetData.HasValue())
    {
        return dotNetData->managedClass;
    }
#endif // HYP_DOTNET

    return nullptr;
}

void ScriptObjectResource::Initialize()
{
    ENGINE_STAT_SCOPE(&s_statInitScriptResource);

#ifdef HYP_DOTNET
    if (dotNetData.HasValue())
    {
        if (!dotNetData->objectPtr)
        {
            return;
        }

        if (dotNetData->objectPtr->SetKeepAlive(true))
        {
            return;
        }

        if (!m_ptr)
        {
            HYP_LOG(Object, Error, "Thread: {}\tManaged object could not be kept alive, it may have been garbage collected\n\tObject address: {}",
                CurrentThreadId().GetName(),
                (void*)dotNetData->objectPtr);

            return;
        }

        // Need to recreate the managed object; could be queued for finalization.
        // In this case, the ref count will be decremented once the queued object is finalized
        const Class* cls = m_ptr->InstanceClass();

        HYP_LOG(Object, Verbose, "Thread: {}\tManaged object for object with Class {} at address {} could not be kept alive, it may have been garbage collected. The managed object will be recreated.\n\tObject address: {}",
            CurrentThreadId().GetName(),
            cls->GetName(), (void*)m_ptr,
            (void*)dotNetData->objectPtr);

        if (dotNetData->managedClass)
        {
            dotnet::ManagedObject* newManagedObject = dotNetData->managedClass->NewObject(cls, m_ptr);

            if (!newManagedObject)
            {
                HYP_FAIL("Failed to recreate managed object for Class %s", cls->GetName().LookupString());
            }

            delete dotNetData->objectPtr;

            // Set the new object pointer
            dotNetData->objectPtr = newManagedObject;
        }
        else
        {
            HYP_FAIL("Failed to recreate managed object for Class %s", cls->GetName().LookupString());
        }
    }
#endif // HYP_DOTNET
}

void ScriptObjectResource::Destroy()
{
#ifdef HYP_DOTNET
    if (dotNetData.HasValue())
    {
        if (dotNetData->objectPtr)
        {
            const DotNETHost& dnh = DotNETHost::GetInstance();

            if (dnh.IsInitialized() && !dnh.IsShuttingDown())
            {
                const bool result = dotNetData->objectPtr->SetKeepAlive(false);
                Assert(result);
            }

           // @NOTE: We do not delete the managed object here nor do we set it as null or unset dotNetData.
           // Instead we just mark it as no longer needing to be kept alive, so it can be collected by the GC if needed.
           // If ref count increments, we mark it as needing to be kept alive again, and if it was collected in the meantime, we recreate it in Initialize().
        }
    }
#endif // HYP_DOTNET
}

#pragma endregion ScriptObjectResource

#ifdef HYP_DOTNET

#pragma region Object Extensions for.NET

ENGINE_API void Object_IncScriptObjectRef(ObjectBase* ptr)
{
    if (!ptr)
    {
        return;
    }

    AssertDebug(ptr->GetObjectHeader_Internal()->GetRefCountStrong() > 1);

    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
    {
        scriptObjectResource->AddReader();
    }
}

ENGINE_API void Object_DecScriptObjectRef(ObjectBase* ptr)
{
    if (!ptr)
    {
        return;
    }

    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
    {
        scriptObjectResource->ReleaseReader();
    }
}

ENGINE_API void Object_ReleaseDotNetGCHandle(ObjectBase* ptr)
{
    if (!ptr)
    {
        return;
    }

    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
    {
        if (auto* dotNetData = scriptObjectResource->GetScriptObjectData_DotNet())
        {
            if (dotNetData->objectPtr)
            {
                dotNetData->objectPtr->SetKeepAlive(false);
            }
        }
    }
}

#pragma endregion // Object Extensions for .NET

#endif // HYP_DOTNET

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
static struct ScriptObjectFunctionsDependencyInject
{
    ScriptObjectFunctionsDependencyInject()
    {
#if defined(HYP_DOTNET) && HYP_DOTNET
        ScriptObjectFunctions::IncScriptObjectRef = &Object_IncScriptObjectRef;
        ScriptObjectFunctions::DecScriptObjectRef = &Object_DecScriptObjectRef;

        ScriptObjectFunctions::ReleaseDotNetGCHandle = &Object_ReleaseDotNetGCHandle;

        ScriptObjectFunctions::CreateScriptObjectResource_DotNet = [](ObjectBase* target, const SharedPtr<dotnet::ManagedClass>& managedClass) -> ScriptObjectResource* {
            return new ScriptObjectResource(target, managedClass);
        };

        ScriptObjectFunctions::ManagedClassSharedThis = [](dotnet::ManagedClass* mc) -> SharedPtr<dotnet::ManagedClass> {
            return mc->SharedThis();
        };

        ScriptObjectFunctions::ManagedClassNewManagedObject = [](dotnet::ManagedClass* mc, void* contextPtr, void (*copyCallback)(void*, void*, unsigned int), dotnet::ObjectReference* outRef) {
            *outRef = mc->NewManagedObject(contextPtr, copyCallback);
        };

        ScriptObjectFunctions::GetManagedObject = [](const ScriptObjectResource* obj) -> dotnet::ManagedObject* {
            return obj->GetManagedObject();
        };
#endif // HYP_DOTNET

#if defined(HYP_SCRIPT) && HYP_SCRIPT
        ScriptObjectFunctions::CreateScriptObjectResource_Script = [](ScriptInstance* instance, ObjectBase* target) -> ScriptObjectResource* {
            return new ScriptObjectResource(instance, target);
        };
#endif // HYP_SCRIPT

        ScriptObjectFunctions::DestroyScriptObjectResource = [](ScriptObjectResource* obj) {
            delete obj;
        };

        ScriptObjectFunctions::GetScriptLanguageMask = [](const ScriptObjectResource* obj) -> unsigned int {
            return obj->GetScriptLanguageMask();
        };
    }
} s_scriptObjectFunctionsDependencyInject {};
#endif

} // namespace Hyperion
