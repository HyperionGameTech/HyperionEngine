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

AnyHandle::AnyHandle(const AnyHandle &other)
    : type_id(other.type_id),
      header(other.header)
{
    if (IsValid()) {
        header->IncRefStrong();
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    const bool was_same_ptr = header == other.header;

    if (!was_same_ptr) {
        if (IsValid()) {
            header->DecRefStrong();
        }
    }

    header = other.header;
    type_id = other.type_id;

    if (!was_same_ptr) {
        if (IsValid()) {
            header->IncRefStrong();
        }
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle &&other) noexcept
    : type_id(other.type_id),
      header(other.header)
{
    other.header = nullptr;
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        header->DecRefStrong();
    }

    header = other.header;
    type_id = other.type_id;

    other.header = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid()) {
        header->DecRefStrong();
    }
}

AnyHandle::IDType AnyHandle::GetID() const
{
    if (!IsValid()) {
        return IDType();
    }

    return IDType { header->index + 1 };
}

AnyRef AnyHandle::ToRef() const &
{
    if (!IsValid()) {
        return AnyRef(type_id, nullptr);
    }

    return AnyRef(type_id, header->container->GetObjectPointer(header));
}

void AnyHandle::Reset()
{
    if (IsValid()) {
        header->DecRefStrong();
    }

    header = nullptr;
}

void *AnyHandle::Release()
{
    if (!IsValid()) {
        return nullptr;
    }

    void *address = header->container->GetObjectPointer(header);
    AssertThrow(address != nullptr);

    header = nullptr;

    return address;
}

#pragma endregion AnyHandle

} // namespace hyperion