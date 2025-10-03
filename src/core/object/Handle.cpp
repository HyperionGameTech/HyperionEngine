/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/Handle.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

HYP_API TypeId GetTypeIdForClass(const HypClass* hypClass)
{
    if (hypClass == nullptr)
    {
        return TypeId::Void();
    }

    return hypClass->GetTypeId();
}

HYP_API HypObjectContainerBase* GetObjectContainerForClass(const HypClass* hypClass)
{
    if (!hypClass)
    {
        return nullptr;
    }

    return hypClass->GetObjectContainer();
}

#pragma region AnyHandle

const AnyHandle AnyHandle::empty = {};

AnyHandle::AnyHandle(HypObjectBase* hypObjectPtr)
    : ptr(hypObjectPtr),
      typeId(hypObjectPtr ? GetTypeIdForClass(hypObjectPtr->m_header->hypClass) : TypeId::Void())
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

AnyHandle::AnyHandle(const HypClass* hypClass, HypObjectBase* ptr)
    : ptr(ptr),
      typeId(hypClass ? hypClass->GetTypeId() : TypeId::Void())
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

    return IdType { ptr->m_header->hypClass->GetTypeId(), ptr->m_header->index + 1 };
}

TypeId AnyHandle::GetTypeId() const
{
    return typeId;
}

AnyRef AnyHandle::ToRef() const
{
    const HypClass* hypClass = ptr ? ptr->m_header->hypClass : nullptr;
    const TypeInfo* typeInfo = hypClass ? &TypeInfo::ForHypClass(hypClass) : &TypeInfo::Void();

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

#pragma endregion AnyHandle

} // namespace hyperion
