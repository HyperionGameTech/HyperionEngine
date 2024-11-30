/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T, sz) \
    UniquePtr<IObjectContainer> *g_container_ptr_##T = ObjectPool::GetObjectContainerHolder().AllotObjectContainer(TypeID::ForType< T >()); \
    UniquePtr<IObjectContainer> *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T, sz) \
    UniquePtr<IObjectContainer> *g_container_ptr_##T = ObjectPool::GetObjectContainerHolder().AllotObjectContainer(TypeID::ForType< ns::T >()); \
    UniquePtr<IObjectContainer> *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

AnyHandle::AnyHandle(TypeID type_id, IDBase id)
    : ptr(nullptr)
{
    if (type_id) {
        IObjectContainer &container = ObjectPool::GetObjectContainerHolder().GetObjectContainer(type_id);

        if (id.IsValid()) {
            ptr = container.GetObjectHeader(id.Value() - 1);
            AssertThrow(ptr != nullptr);

            ptr->IncRefStrong();
        } else {
            ptr = container.GetDefaultObjectHeader();
        }
    }
}

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

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->DecRefStrong();
    }

    ptr = other.ptr;
    other.ptr = nullptr;

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