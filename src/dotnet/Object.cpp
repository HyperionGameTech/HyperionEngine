/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNetSystem.hpp>

namespace hyperion::dotnet {

Object::Object()
    : m_class_ptr(nullptr),
      m_object_reference { ManagedGuid { 0, 0 }, nullptr },
      m_object_flags(ObjectFlags::NONE)
{
}

Object::Object(const RC<Class> &class_ptr, ObjectReference object_reference, EnumFlags<ObjectFlags> object_flags)
    : m_class_ptr(class_ptr),
      m_object_reference(object_reference),
      m_object_flags(object_flags)
{
    if (class_ptr != nullptr) {
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
        m_assembly = class_ptr->GetAssembly();
#else
        m_assembly = class_ptr->GetAssembly().ToWeak();
#endif
    }

    if (m_object_reference.guid.IsValid()) {
        AssertThrowMsg(m_class_ptr != nullptr, "Class pointer not set!");
    }
}

// Object::Object(Object &&other) noexcept
//     : m_class_ptr(std::move(other.m_class_ptr)),
//       m_assembly(std::move(other.m_assembly)),
//       m_object_reference(other.m_object_reference),
//       m_object_flags(other.m_object_flags)
// {
//     other.m_object_reference = ObjectReference { ManagedGuid { 0, 0 }, nullptr };
//     other.m_object_flags = ObjectFlags::NONE;
// }

// Object &Object::operator=(Object &&other) noexcept
// {
//     if (this == &other) {
//         return *this;
//     }

//     Reset();

//     m_class_ptr = std::move(other.m_class_ptr);
//     m_object_reference = other.m_object_reference;
//     m_object_flags = other.m_object_flags;

//     other.m_object_reference = ObjectReference { ManagedGuid { 0, 0 }, nullptr };
//     other.m_object_flags = ObjectFlags::NONE;

//     return *this;
// }

Object::~Object()
{
    Reset();
}

void Object::Reset()
{
    if (IsValid()) {
        AssertThrowMsg(!(m_object_flags & ObjectFlags::KEEP_ALIVE), "Object is being destroyed while KEEP_ALIVE flag is set!");
    }

    m_class_ptr.Reset();
    m_assembly.Reset();
    m_object_reference = ObjectReference { ManagedGuid { 0, 0 }, nullptr };
    m_object_flags = ObjectFlags::NONE;
}

void Object::InvokeMethod_Internal(const Method *method_ptr, const HypData **args_hyp_data, HypData *out_return_hyp_data)
{
    AssertThrow(IsValid());

#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    const RC<Assembly> &assembly = m_assembly;
#else
    RC<Assembly> assembly = m_assembly.Lock();
#endif

    AssertThrow(assembly != nullptr && assembly->IsLoaded());
    
    AssertThrow(method_ptr->GetFunctionPointer() != nullptr);
    method_ptr->GetFunctionPointer()(method_ptr->GetGuid(), m_object_reference.guid, args_hyp_data, out_return_hyp_data);
}

const Method *Object::GetMethod(UTF8StringView method_name) const
{
    if (!IsValid()) {
        return nullptr;
    }

    auto it = m_class_ptr->GetMethods().FindAs(method_name);

    if (it == m_class_ptr->GetMethods().End()) {
        return nullptr;
    }

    return &it->second;
}

const Property *Object::GetProperty(UTF8StringView property_name) const
{
    if (!IsValid()) {
        return nullptr;
    }

    auto it = m_class_ptr->GetProperties().FindAs(property_name);

    if (it == m_class_ptr->GetProperties().End()) {
        return nullptr;
    }

    return &it->second;
}

bool Object::SetKeepAlive(bool keep_alive)
{
    AssertThrow(bool(m_object_flags & ObjectFlags::KEEP_ALIVE) != keep_alive);

    if (!DotNetSystem::GetInstance().GetGlobalFunctions().set_object_reference_type_function(&m_object_reference, !keep_alive)) {
        return false;
    }

    m_object_flags[ObjectFlags::KEEP_ALIVE] = keep_alive;

    return true;
}

} // namespace hyperion::dotnet