/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion::v2 {

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

} // namespace hyperion::v2