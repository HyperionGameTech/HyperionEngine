/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< T >()); \
    IObjectContainer *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< ns::T >()); \
    IObjectContainer *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

const AnyHandle AnyHandle::empty = { };

AnyHandle::AnyHandle(HypObjectBase *hyp_object_ptr)
    : type_id(hyp_object_ptr ? hyp_object_ptr->GetObjectHeader_Internal()->container->GetObjectTypeID() : TypeID::Void()),
      ptr(hyp_object_ptr)
{
    if (IsValid()) {
        ptr->GetObjectHeader_Internal()->IncRefStrong();
    }
}

AnyHandle::AnyHandle(const AnyHandle &other)
    : type_id(other.type_id),
      ptr(other.ptr)
{
    if (IsValid()) {
        ptr->GetObjectHeader_Internal()->IncRefStrong();
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    const bool was_same_ptr = ptr == other.ptr;

    if (!was_same_ptr) {
        if (IsValid()) {
            ptr->GetObjectHeader_Internal()->DecRefStrong();
        }
    }

    ptr = other.ptr;
    type_id = other.type_id;

    if (!was_same_ptr) {
        if (IsValid()) {
            ptr->GetObjectHeader_Internal()->IncRefStrong();
        }
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle &&other) noexcept
    : type_id(other.type_id),
      ptr(other.ptr)
{
    other.ptr = nullptr;
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->GetObjectHeader_Internal()->DecRefStrong();
    }

    ptr = other.ptr;
    type_id = other.type_id;

    other.ptr = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid()) {
        ptr->GetObjectHeader_Internal()->DecRefStrong();
    }
}

AnyHandle::IDType AnyHandle::GetID() const
{
    if (!IsValid()) {
        return IDType();
    }

    return IDType { ptr->GetObjectHeader_Internal()->index + 1 };
}

AnyRef AnyHandle::ToRef() const
{
    if (!IsValid()) {
        return AnyRef(type_id, nullptr);
    }

    HypObjectHeader *header = ptr->GetObjectHeader_Internal();

    return AnyRef(type_id, header->container->GetObjectPointer(header));
}

void AnyHandle::Reset()
{
    if (IsValid()) {
        ptr->GetObjectHeader_Internal()->DecRefStrong();
    }

    ptr = nullptr;
}

void *AnyHandle::Release()
{
    if (!IsValid()) {
        return nullptr;
    }

    HypObjectHeader *header = ptr->GetObjectHeader_Internal();
    void *address = header->container->GetObjectPointer(header);

    ptr = nullptr;

    return address;
}

#pragma endregion AnyHandle

} // namespace hyperion