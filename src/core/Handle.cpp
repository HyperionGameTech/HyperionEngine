/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T, sz) \
    UniquePtr<ObjectContainerBase> *g_container_ptr_##T = AllotContainer< T >(); \
    UniquePtr<ObjectContainerBase> *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T, sz) \
    UniquePtr<ObjectContainerBase> *g_container_ptr_##T = AllotContainer< ns::T >(); \
    UniquePtr<ObjectContainerBase> *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

AnyHandle::AnyHandle(TypeID type_id, IDBase id)
    : type_id(type_id),
      index(id.Value())
{
    if (type_id && index != 0) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        container.IncRefStrong(index - 1);
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    if (type_id && index != 0) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        container.DecRefStrong(index - 1);
    }

    type_id = other.type_id;
    index = other.index;

    if (type_id && index != 0) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        container.IncRefStrong(index - 1);
    }

    return *this;
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (type_id && index != 0) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        container.DecRefStrong(index - 1);
    }

    type_id = other.type_id;
    index = other.index;

    other.type_id = TypeID::Void();
    other.index = 0;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (type_id && index != 0) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        container.DecRefStrong(index - 1);
    }
}

AnyRef AnyHandle::ToRef() const
{
    if (!type_id || index == 0) {
        return AnyRef();
    }

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

    return AnyRef(type_id, container.GetObjectPointer(index - 1));
}

#pragma endregion AnyHandle

} // namespace hyperion