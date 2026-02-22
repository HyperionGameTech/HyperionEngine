/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/Class.hpp>

namespace Hyperion {

HYP_API TypeId GetTypeIdForClass(const Class* cls)
{
    if (cls == nullptr)
    {
        return TypeId::Void();
    }

    return cls->GetTypeId();
}

HYP_API ObjectContainerBase* GetObjectContainerForClass(const Class* cls)
{
    if (!cls)
    {
        return nullptr;
    }

    return cls->GetObjectContainer();
}

#pragma region Handle < ObjectBase>

const AnyHandle AnyHandle::empty = {};

AnyHandle::AnyHandle(ObjectBase* obj)
    : ptr(obj),
      typeId(obj ? obj->m_header->cls->GetTypeId() : TypeId::Void())
{
    if (IsValid())
    {
        if (!ptr->m_header->TryIncRefStrong())
        {
            // not alive; unset
            ptr = nullptr;
            typeId = TypeId::Void();
        }
    }
}

AnyHandle::AnyHandle(const Class* cls, ObjectBase* ptr)
    : ptr(ptr),
      typeId(cls ? cls->GetTypeId() : TypeId::Void())
{
    if (IsValid())
    {
        if (!ptr->m_header->TryIncRefStrong())
        {
            // not alive; unset
            ptr = nullptr;
            typeId = TypeId::Void();
        }
    }
}

AnyHandle::AnyHandle(const AnyHandle& other)
    : typeId(other.typeId),
      ptr(other.ptr)
{
    if (IsValid())
    {
        ptr->m_header->IncRefStrong();
    }
}

AnyHandle& AnyHandle::operator=(const AnyHandle& other)
{
    if (this == &other)
    {
        return *this;
    }

    const bool wasSamePtr = ptr == other.ptr;

    if (!wasSamePtr)
    {
        if (IsValid())
        {
            ptr->m_header->DecRefStrong();
        }
    }

    ptr = other.ptr;
    typeId = other.typeId;

    if (!wasSamePtr)
    {
        if (IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle&& other) noexcept
    : typeId(other.typeId),
      ptr(other.ptr)
{
    other.ptr = nullptr;
}

AnyHandle& AnyHandle::operator=(AnyHandle&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }

    ptr = other.ptr;
    typeId = other.typeId;

    other.ptr = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }
}

AnyHandle::IdType AnyHandle::Id() const
{
    if (!IsValid())
    {
        return IdType();
    }

    return IdType { ptr->m_header->cls->GetTypeId(), ptr->m_header->index + 1 };
}

TypeId AnyHandle::GetTypeId() const
{
    return typeId;
}

AnyRef AnyHandle::ToRef() const
{
    const Class* cls = ptr ? ptr->m_header->cls : nullptr;
    const TypeInfo* typeInfo = cls ? cls->GetTypeInfo() : &TypeInfo_Void();

    if (!IsValid())
    {
        return AnyRef(typeInfo, nullptr);
    }

    return AnyRef(typeInfo, ptr);
}

void AnyHandle::Reset()
{
    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }

    ptr = nullptr;
}

void* AnyHandle::Release()
{
    if (!IsValid())
    {
        return nullptr;
    }

    void* address = ptr;
    ptr = nullptr;

    return address;
}

#pragma endregion Handle < ObjectBase>

} // namespace Hyperion
