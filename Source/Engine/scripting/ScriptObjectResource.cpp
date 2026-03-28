#include <HyperionPch.hpp>

#include <scripting/ScriptObjectResource.hpp>
#include <scripting/Script.hpp>

#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Object.hpp>

#include <Core/debug/Debug.hpp>

#ifdef HYP_DOTNET
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/DotNETHost.hpp>
#endif

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

namespace Hyperion {

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

ScriptObjectResource::ScriptObjectResource(dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass)
    : m_ptr(nullptr)
{
#ifdef HYP_DOTNET
    if (!dotNetData)
    {
        dotNetData.Emplace(ScriptObjectData_DotNet());
    }

    ScriptObjectData_DotNet& data = *dotNetData;
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;

    AssertDebug(data.objectPtr && data.managedClass);
#endif
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, const RC<dotnet::ManagedClass>& managedClass)
    : ScriptObjectResource(ptr, managedClass, {}, ObjectFlags::NONE)
{
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass)
    : m_ptr(ptr)
{
#ifdef HYP_DOTNET
    if (!dotNetData)
    {
        dotNetData.Emplace(ScriptObjectData_DotNet());
    }

    ScriptObjectData_DotNet& data = *dotNetData;
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;

    AssertDebug(data.objectPtr && data.managedClass);
#endif
}

ScriptObjectResource::ScriptObjectResource(ObjectBase* ptr, const RC<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_ptr(ptr)
{
#ifdef HYP_DOTNET
    if (!dotNetData)
    {
        dotNetData.Emplace(ScriptObjectData_DotNet());
    }

    ScriptObjectData_DotNet& data = *dotNetData;
    data.objectPtr = nullptr;
    data.managedClass = managedClass;

    AssertDebug(m_ptr && managedClass);

    if (m_ptr && managedClass)
    {
        if (objectFlags & ObjectFlags::CREATED_FROM_MANAGED)
        {
            data.objectPtr = new dotnet::ManagedObject(managedClass->RefCountedPtrFromThis(), objectReference, ObjectFlags::CREATED_FROM_MANAGED);
        }
        else
        {
            data.objectPtr = managedClass->NewObject(m_ptr->InstanceClass(), m_ptr);
        }

        Assert(data.objectPtr != nullptr);
    }
#endif
}

#ifdef HYP_SCRIPT

ScriptObjectResource::ScriptObjectResource(ScriptInstance* hypScriptInstance, BoxedValue&& hypScriptValue)
    : m_ptr(nullptr)
{
    if (!hypScriptData)
    {
        hypScriptData.Emplace(ScriptObjectData_HypScript());
    }

    ScriptObjectData_HypScript& data = *hypScriptData;
    data.instance = hypScriptInstance;
    data.obj = std::move(hypScriptValue);
}

#endif

ScriptObjectResource::~ScriptObjectResource()
{
#ifdef HYP_SCRIPT
    if (hypScriptData.HasValue())
    {
        if (hypScriptData->instance)
        {
            HypScript::GetInstance().DestroyScript(hypScriptData->instance);
            hypScriptData->instance = nullptr;
        }

        hypScriptData->obj = BoxedValue();

        hypScriptData.Unset();
    }
#endif
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
#endif

#ifdef HYP_SCRIPT
    if (hypScriptData.HasValue())
    {
        mask |= (1 << uint32(ScriptLanguage::HypScript));
    }
#endif

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
#endif

    return nullptr;
}

const RC<dotnet::ManagedClass> ScriptObjectResource::GetManagedClass() const
{
#ifdef HYP_DOTNET
    // only valid to call on .NET script objects
    if (dotNetData.HasValue())
    {
        return dotNetData->managedClass;
    }
#endif

    return nullptr;
}

void ScriptObjectResource::Initialize()
{
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
#endif
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
        }
    }
#endif
}

#pragma endregion ScriptObjectResource

#ifdef HYP_DOTNET

#pragma region Object Extensions for.NET

HYP_API void Object_IncScriptObjectRef(ObjectBase* ptr)
{
    AssertDebug(ptr->GetObjectHeader_Internal()->GetRefCountStrong() > 1);

    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
    {
        scriptObjectResource->AddReader();
    }
}

HYP_API void Object_DecScriptObjectRef(ObjectBase* ptr)
{
    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
    {
        scriptObjectResource->ReleaseReader();
    }
}

#pragma endregion // Object Extensions for .NET

#endif

} // namespace Hyperion
