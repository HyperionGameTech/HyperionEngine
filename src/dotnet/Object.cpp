/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

Object::Object()
    : m_class_ptr(nullptr),
      m_object_reference { nullptr, nullptr },
      m_object_flags(ObjectFlags::NONE),
      m_keep_alive(false)
{
}

Object::Object(const RC<Class>& class_ptr, ObjectReference object_reference, EnumFlags<ObjectFlags> object_flags)
    : m_class_ptr(class_ptr),
      m_object_reference(object_reference),
      m_object_flags(object_flags),
      m_keep_alive(false)
{
    if (class_ptr != nullptr)
    {
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
        m_assembly = class_ptr->GetAssembly();
#else
        m_assembly = class_ptr->GetAssembly().ToWeak();
#endif
    }

    if (m_object_reference.weak_handle != nullptr)
    {
        AssertThrowMsg(m_class_ptr != nullptr, "Class pointer not set!");

        if (!(m_object_flags & ObjectFlags::CREATED_FROM_MANAGED))
        {
            // set keep_alive to true if reference is valid so we can clean up on destructor.
            // If we call this constructor the managed object should be alive anyway (See NativeInterop.cs)
            m_keep_alive.Set(true, MemoryOrder::RELEASE);
        }
    }
}

Object::~Object()
{
    Reset();
}

void Object::Reset()
{
    HYP_MT_CHECK_RW(m_data_race_detector);

    if (IsValid() && m_keep_alive.Get(MemoryOrder::ACQUIRE))
    {
        AssertThrowMsg(SetKeepAlive(false), "Failed to set keep alive to false!");
    }

    m_class_ptr.Reset();
    m_assembly.Reset();
    m_object_reference = ObjectReference { nullptr, nullptr };
    m_object_flags = ObjectFlags::NONE;
    m_keep_alive.Set(false, MemoryOrder::RELEASE);
}

void Object::InvokeMethod_Internal(const Method* method_ptr, const HypData** args_hyp_data, HypData* out_return_hyp_data)
{
    AssertThrow(IsValid());

#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    const RC<Assembly>& assembly = m_assembly;
#else
    RC<Assembly> assembly = m_assembly.Lock();
#endif

    AssertThrow(assembly != nullptr && assembly->IsLoaded());

    method_ptr->Invoke(&m_object_reference, args_hyp_data, out_return_hyp_data);
}

const Method* Object::GetMethod(UTF8StringView method_name) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_class_ptr->GetMethods().FindAs(method_name);

    if (it == m_class_ptr->GetMethods().End())
    {
        return nullptr;
    }

    return &it->second;
}

const Property* Object::GetProperty(UTF8StringView property_name) const
{
    if (!IsValid())
    {
        return nullptr;
    }

    auto it = m_class_ptr->GetProperties().FindAs(property_name);

    if (it == m_class_ptr->GetProperties().End())
    {
        return nullptr;
    }

    return &it->second;
}

bool Object::SetKeepAlive(bool keep_alive)
{
    if (!IsValid())
    {
        return false;
    }

    if (m_keep_alive.Get(MemoryOrder::ACQUIRE) == keep_alive)
    {
        return true;
    }

    // used as result (inout parameter)
    int param_result = int(keep_alive);
    DotNetSystem::GetInstance().GetGlobalFunctions().set_keep_alive_function(&m_object_reference, &param_result);

    if (param_result)
    {
        m_keep_alive.Set(keep_alive, MemoryOrder::RELEASE);

        return true;
    }

    return false;
}

} // namespace hyperion::dotnet