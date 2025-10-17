#include <scripting/ScriptObjectResource.hpp>
#include <scripting/Script.hpp>

#include <core/reflection/HypClass.hpp>
#include <core/reflection/HypClassRegistry.hpp>
#include <core/reflection/HypObject.hpp>

#include <core/logging/Logger.hpp>

#include <core/debug/Debug.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Resource);
HYP_DECLARE_LOG_CHANNEL(Object);

#pragma region ScriptObjectResource

ScriptObjectResource::ScriptObjectResource() = default;

ScriptObjectResource::ScriptObjectResource(dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass)
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet& data = m_scriptObjectData.Emplace<ScriptObjectData_DotNet>();
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;
#endif
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::ManagedClass>& managedClass)
    : ScriptObjectResource(ptr, managedClass, {}, ObjectFlags::NONE)
{
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass)
    : m_ptr(ptr)
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet& data = m_scriptObjectData.Emplace<ScriptObjectData_DotNet>();
    data.objectPtr = objectPtr;
    data.managedClass = managedClass;
#endif
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_ptr(ptr)
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet& data = m_scriptObjectData.Emplace<ScriptObjectData_DotNet>();
    data.objectPtr = nullptr;
    data.managedClass = managedClass;

    if (m_ptr && managedClass)
    {
        void* address = m_ptr.GetPointer();

        if (objectFlags & ObjectFlags::CREATED_FROM_MANAGED)
        {
            data.objectPtr = new dotnet::ManagedObject(managedClass->RefCountedPtrFromThis(), objectReference, ObjectFlags::CREATED_FROM_MANAGED);
        }
        else
        {
            HYP_LOG(Object, Debug, "Creating new managed object with class {}, reference will be incremented from C#", managedClass->GetName());

            data.objectPtr = managedClass->NewObject(m_ptr.GetClass(), address);
        }

        HYP_CORE_ASSERT(data.objectPtr != nullptr);
    }
#endif
}

#ifdef HYP_SCRIPT

ScriptObjectResource::ScriptObjectResource(Script_Instance* hypScriptInstance, HypData&& hypScriptValue)
{
    ScriptObjectData_HypScript& data = m_scriptObjectData.Emplace<ScriptObjectData_HypScript>();
    data.instance = hypScriptInstance;
    data.obj = std::move(hypScriptValue);
}

#endif

ScriptObjectResource::~ScriptObjectResource()
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet* dotNetData = GetScriptObjectData_DotNet();

    if (dotNetData)
    {
        if (dotNetData->objectPtr)
        {
            delete dotNetData->objectPtr;
            dotNetData->objectPtr = nullptr;
        }

        dotNetData->managedClass = nullptr;
    }
#endif

#ifdef HYP_SCRIPT
    ScriptObjectData_HypScript* hypScriptData = GetScriptObjectData_HypScript();

    if (hypScriptData)
    {
        if (hypScriptData->instance)
        {
            HypScript::GetInstance().DestroyScript(hypScriptData->instance);
            hypScriptData->instance = nullptr;
        }

        hypScriptData->obj = HypData();
    }
#endif

    m_scriptObjectData.Reset();
}

ScriptLanguage ScriptObjectResource::GetScriptLanguage() const
{
#if !defined(HYP_DOTNET) && !defined(HYP_SCRIPT)
    return SL_INVALID;
#else
    ScriptLanguage language = SL_INVALID;

    Visit(m_scriptObjectData, [&language](auto&& data)
        {
            language = data.Language;
        });

    return language;
#endif
}

void ScriptObjectResource::Initialize()
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet* dotNetData = GetScriptObjectData_DotNet();

    if (dotNetData != nullptr)
    {
        if (!dotNetData->objectPtr)
        {
            return;
        }

        if (dotNetData->objectPtr->SetKeepAlive(true))
        {
            return;
        }

        if (!m_ptr.IsValid())
        {
            HYP_LOG(Object, Error, "Thread: {}\tManaged object could not be kept alive, it may have been garbage collected\n\tObject address: {}",
                Threads::CurrentThreadId().GetName(),
                (void*)dotNetData->objectPtr);

            return;
        }

        // Need to recreate the managed object; could be queued for finalization.
        // In this case, the ref count will be decremented once the queued object is finalized
        const HypClass* hypClass = m_ptr.GetClass();

        HYP_LOG(Object, Info, "Thread: {}\tManaged object for object with HypClass {} at address {} could not be kept alive, it may have been garbage collected. The managed object will be recreated.\n\tObject address: {}",
            Threads::CurrentThreadId().GetName(),
            hypClass->GetName(), m_ptr.GetPointer(),
            (void*)dotNetData->objectPtr);

        if (dotNetData->managedClass)
        {
            dotnet::ManagedObject* newManagedObject = dotNetData->managedClass->NewObject(hypClass, m_ptr.GetPointer());

            if (!newManagedObject)
            {
                HYP_FAIL("Failed to recreate managed object for HypClass %s", hypClass->GetName().LookupString());
            }

            delete dotNetData->objectPtr;

            // Set the new object pointer
            dotNetData->objectPtr = newManagedObject;
        }
        else
        {
            HYP_FAIL("Failed to recreate managed object for HypClass %s", hypClass->GetName().LookupString());
        }
    }
#endif
}

void ScriptObjectResource::Destroy()
{
#ifdef HYP_DOTNET
    ScriptObjectData_DotNet* dotNetData = GetScriptObjectData_DotNet();

    if (dotNetData)
    {
        if (dotNetData->objectPtr)
        {
            const bool result = dotNetData->objectPtr->SetKeepAlive(false);

            HYP_CORE_ASSERT(result);

            delete dotNetData->objectPtr;
            dotNetData->objectPtr = nullptr;
        }

        dotNetData->managedClass = nullptr;
    }
#endif

    m_scriptObjectData.Reset();
}

#pragma endregion ScriptObjectResource

#ifdef HYP_DOTNET

#pragma region HypObject Extensions for .NET

HYP_API void HypObject_IncScriptObjectRef(HypObjectBase* ptr)
{
    AssertDebug(ptr->GetObjectHeader_Internal()->GetRefCountStrong() > 1);

    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguage() == SL_CSHARP)
    {
        scriptObjectResource->IncRef();
    }
}

HYP_API void HypObject_DecScriptObjectRef(HypObjectBase* ptr)
{
    if (ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        scriptObjectResource && scriptObjectResource->GetScriptLanguage() == SL_CSHARP)
    {
        scriptObjectResource->DecRef();
    }
}

#pragma endregion // HypObject Extensions for .NET

#endif

} // namespace hyperion
