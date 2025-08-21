/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

Object::Object()
    : m_classPtr(nullptr),
      m_objectReference { nullptr, nullptr },
      m_objectFlags(ObjectFlags::NONE),
      m_keepAlive(false)
{
}

Object::Object(const RC<Class>& classPtr, ObjectReference objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_classPtr(classPtr),
      m_objectReference(objectReference),
      m_objectFlags(objectFlags),
      m_keepAlive(false)
{
    if (classPtr != nullptr)
    {
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
        m_assembly = classPtr->GetAssembly();
#else
        m_assembly = classPtr->GetAssembly().ToWeak();
#endif
    }

    if (m_objectReference.weakHandle != nullptr)
    {
        Assert(m_classPtr != nullptr, "Class pointer not set!");

        if (!(m_objectFlags & ObjectFlags::CREATED_FROM_MANAGED))
        {
            // set keepAlive to true if reference is valid so we can clean up on destructor.
            // If we call this constructor the managed object should be alive anyway (See NativeInterop.cs)
            m_keepAlive.Set(true, MemoryOrder::RELEASE);
        }
    }
}

Object::~Object()
{
    Reset();
}

void Object::Reset()
{
    if (IsValid() && m_keepAlive.Get(MemoryOrder::ACQUIRE))
    {
        Assert(SetKeepAlive(false), "Failed to set keep alive to false!");
    }

    m_classPtr.Reset();
    m_assembly.Reset();
    m_objectReference = ObjectReference { nullptr, nullptr };
    m_objectFlags = ObjectFlags::NONE;
    m_keepAlive.Set(false, MemoryOrder::RELEASE);
}

void Object::InvokeMethod_Internal(const Method* methodPtr, const HypData** argsHypData, HypData* outReturnHypData)
{
    Assert(IsValid());

#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    const RC<Assembly>& assembly = m_assembly;
#else
    RC<Assembly> assembly = m_assembly.Lock();
#endif

    Assert(assembly != nullptr && assembly->IsLoaded());

    methodPtr->Invoke(&m_objectReference, argsHypData, outReturnHypData);
}

const Method* Object::GetMethod(UTF8StringView methodName) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_classPtr->GetMethods().FindAs(methodName);

    if (it == m_classPtr->GetMethods().End())
    {
        return nullptr;
    }

    return &it->second;
}

const Property* Object::GetProperty(UTF8StringView propertyName) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_classPtr->GetProperties().FindAs(propertyName);

    if (it == m_classPtr->GetProperties().End())
    {
        return nullptr;
    }

    return &it->second;
}

bool Object::SetKeepAlive(bool keepAlive)
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
    DotNetSystem::GetInstance().GetGlobalFunctions().setKeepAliveFunction(&m_objectReference, &paramResult);

    if (paramResult)
    {
        m_keepAlive.Set(keepAlive, MemoryOrder::RELEASE);

        return true;
    }

    return false;
}

} // namespace hyperion::dotnet
