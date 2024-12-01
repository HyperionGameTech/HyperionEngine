/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T, sz) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< T >(), []() -> UniquePtr<IObjectContainer> { return MakeUnique<ObjectContainer< T >>(); }); \
    IObjectContainer *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T, sz) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< ns::T >(), []() -> UniquePtr<IObjectContainer> { return MakeUnique<ObjectContainer< ns::T >>(); }); \
    IObjectContainer *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

AnyHandle::AnyHandle(IHypObject *hyp_object_ptr)
    : ptr(nullptr)
{
    if (hyp_object_ptr) {
        ptr = hyp_object_ptr->GetObjectHeader_Internal();

        ptr->IncRefStrong();
    }
}

AnyHandle::AnyHandle(const AnyHandle &other)
    : ptr(other.ptr)
{
    if (IsValid()) {
        ptr->IncRefStrong();
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->DecRefStrong();
    }

    ptr = other.ptr;

    if (IsValid()) {
        ptr->IncRefStrong();
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle &&other) noexcept
    : ptr(other.ptr)
{
    if (other.IsValid()) {
        IObjectContainer &container = ObjectPool::GetObjectContainerHolder().Get(other.GetTypeID());
        other.ptr = container.GetDefaultObjectHeader();
    }
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->DecRefStrong();
    }

    if (other.IsValid()) {
        IObjectContainer &container = ObjectPool::GetObjectContainerHolder().Get(other.GetTypeID());
        other.ptr = container.GetDefaultObjectHeader();
    }

    ptr = other.ptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid()) {
        ptr->DecRefStrong();
    }
}

AnyHandle::IDType AnyHandle::GetID() const
{
    if (!IsValid()) {
        return IDType();
    }

    return IDType { ptr->index + 1 };
}

TypeID AnyHandle::GetTypeID() const
{
    if (ptr == nullptr) {
        return TypeID::Void();
    }

    return ptr->container->GetObjectTypeID();
}

AnyRef AnyHandle::ToRef() const
{
    if (ptr == nullptr) {
        return AnyRef(TypeID::Void(), nullptr);
    }

    if (ptr->IsNull()) {
        return AnyRef(ptr->container->GetObjectTypeID(), nullptr);
    }

    return ptr->container->GetObject(ptr);
}

#pragma endregion AnyHandle

} // namespace hyperion