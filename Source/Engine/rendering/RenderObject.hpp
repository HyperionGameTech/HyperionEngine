/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

#define DECLARE_GFX_TYPE_BASE(T)        \
    class T##Base;                      \
                                        \
    using T##BaseRef = Handle<T##Base>; \
    using T##BaseWeakRef = WeakHandle<T##Base>;

#if HYP_VULKAN
#define DECLARE_GFX_TYPE(T)                                                           \
    DECLARE_GFX_TYPE_BASE(T);                                                         \
                                                                                      \
    class Vulkan##T;                                                                  \
                                                                                      \
    using T = Vulkan##T;                                                              \
                                                                                      \
    using Vulkan##T##Ref = Handle<Vulkan##T>;                                         \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;                                 \
                                                                                      \
    using T##Ref = Vulkan##T##Ref;                                                    \
    using T##WeakRef = Vulkan##T##WeakRef;                                            \

#define DECLARE_VULKAN_GFX_TYPE(T)            \
    class Vulkan##T;                          \
                                              \
    using Vulkan##T##Ref = Handle<Vulkan##T>; \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;

#elif HYP_DX12
#define DECLARE_GFX_TYPE(T)                                                           \
    DECLARE_GFX_TYPE_BASE(T);                                                         \
                                                                                      \
    class DX12##T;                                                                   \
                                                                                      \
    using T = DX12##T;                                                               \
                                                                                      \
    using DX12##T##Ref = Handle<DX12##T>;                                            \
    using DX12##T##WeakRef = WeakHandle<DX12##T>;                                    \
                                                                                      \
    using T##Ref = DX12##T##Ref;                                                     \
    using T##WeakRef = DX12##T##WeakRef;                                             \

#define DECLARE_DX12_GFX_TYPE(T)             \
    class DX12##T;                           \
                                              \
    using DX12##T##Ref = Handle<DX12##T>;    \
    using DX12##T##WeakRef = WeakHandle<DX12##T>;
#endif


#include <rendering/inl/RenderObjectDefinitions.inl>

} // namespace Hyperion
