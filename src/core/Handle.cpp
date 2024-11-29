/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T, sz) \
    UniquePtr<IObjectContainer> *g_container_ptr_##T = AllotContainer< T >(); \
    UniquePtr<IObjectContainer> *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T, sz) \
    UniquePtr<IObjectContainer> *g_container_ptr_##T = AllotContainer< ns::T >(); \
    UniquePtr<IObjectContainer> *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

AnyHandle::AnyHandle(TypeID type_id, IDBase id)
    : container(&ObjectPool::GetContainer(type_id)),
      ptr(nullptr)
{
    if (id.IsValid()) {
        ptr = container->GetObjectBytes(id.Value() - 1);

        container->IncRefStrong(ptr);
    }
}

AnyHandle::AnyHandle(const AnyHandle &other)
    : container(other.container),
      ptr(other.ptr)
{
    if (ptr) {
        container->IncRefStrong(ptr);
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        container->DecRefStrong(ptr);
    }

    container = other.container;
    ptr = other.ptr;

    if (IsValid()) {
        container->IncRefStrong(ptr);
    }

    return *this;
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        container->DecRefStrong(ptr);
    }

    container = other.container;
    ptr = other.ptr;

    other.container = nullptr;
    other.ptr = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid()) {
        container->DecRefStrong(ptr);
    }
}

AnyHandle::IDType AnyHandle::GetID() const
{
    if (!IsValid()) {
        return IDType();
    }

    return IDType { container->GetObjectIndex(ptr) + 1 };
}

TypeID AnyHandle::GetTypeID() const
{
    if (!container) {
        return TypeID::Void();
    }

    return container->GetObjectTypeID();
}

AnyRef AnyHandle::ToRef() const
{
    if (!container) {
        return AnyRef(TypeID::Void(), nullptr);
    }

    return container->GetObject(ptr);
}

#pragma endregion AnyHandle

} // namespace hyperion