/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNETHost.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

ManagedObject::ManagedObject()
    : m_managedClass(nullptr),
      m_objectReference { nullptr, nullptr },
      m_objectFlags(ObjectFlags::NONE),
      m_keepAlive(false)
{
}

ManagedObject::ManagedObject(const RC<ManagedClass>& managedClass, ObjectReference objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_managedClass(managedClass),
      m_objectReference(objectReference),
      m_objectFlags(objectFlags),
      m_keepAlive(false)
{
    if (managedClass != nullptr)
    {
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
        m_assembly = managedClass->GetAssembly();
#else
        m_assembly = managedClass->GetAssembly().ToWeak();
#endif
    }

    if (m_objectReference.weakHandle != nullptr)
    {
        Assert(m_managedClass != nullptr, "Class pointer not set!");

        if (!(m_objectFlags & ObjectFlags::CREATED_FROM_MANAGED))
        {
            // set keepAlive to true if reference is valid so we can clean up on destructor.
            // If we call this constructor the managed object should be alive anyway (See NativeInterop.cs)
            m_keepAlive.Set(true, MemoryOrder::RELEASE);
        }
    }
}

ManagedObject::~ManagedObject()
{
    Reset();
}

void ManagedObject::Reset()
{
    if (IsValid() && m_keepAlive.Get(MemoryOrder::ACQUIRE))
    {
        Assert(SetKeepAlive(false), "Failed to set keep alive to false!");
    }

    m_managedClass.Reset();
    m_assembly.Reset();
    m_objectReference = ObjectReference { nullptr, nullptr };
    m_objectFlags = ObjectFlags::NONE;
    m_keepAlive.Set(false, MemoryOrder::RELEASE);
}

void ManagedObject::InvokeMethod_Internal(const ManagedMethod* pMethod, const BoxedValue** ppArgs, BoxedValue* pOutReturn)
{
    Assert(IsValid());

#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    const RC<Assembly>& assembly = m_assembly;
#else
    RC<Assembly> assembly = m_assembly.Lock();
#endif

    Assert(assembly != nullptr && assembly->IsLoaded());

    pMethod->Invoke(&m_objectReference, ppArgs, pOutReturn);
}

const ManagedMethod* ManagedObject::GetMethod(ANSIStringView methodName) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_managedClass->GetMethods().FindAs(methodName);

    if (it == m_managedClass->GetMethods().End())
    {
        return nullptr;
    }

    return &it->second;
}

const ManagedProperty* ManagedObject::GetProperty(ANSIStringView propertyName) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_managedClass->GetProperties().FindAs(propertyName);

    if (it == m_managedClass->GetProperties().End())
    {
        return nullptr;
    }

    return &it->second;
}

bool ManagedObject::SetKeepAlive(bool keepAlive)
{
    if (!IsValid())
    {
        return false;
    }

    if (m_keepAlive.Get(MemoryOrder::ACQUIRE) == keepAlive)
    {
        return true;
    }

    // used as result (inout parameter)
    int paramResult = int(keepAlive);
    DotNETHost::GetInstance().GetGlobalFunctions().setKeepAliveFunction(&m_objectReference, &paramResult);

    if (paramResult)
    {
        m_keepAlive.Set(keepAlive, MemoryOrder::RELEASE);

        return true;
    }

    return false;
}

} // namespace hyperion::dotnet
