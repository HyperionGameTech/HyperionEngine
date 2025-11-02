/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/HashCode.hpp>
#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <core/reflection/Handle.hpp>

#include <core/threading/Threads.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderResult.hpp>

namespace hyperion {

#define DECLARE_SHARED_GFX_TYPE(T)                                                    \
    class T##Base;                                                                    \
    class Vulkan##T;                                                                  \
                                                                                      \
    using T##Ref = Handle<T##Base>;                                                   \
    using T##WeakRef = WeakHandle<T##Base>;                                           \
                                                                                      \
    using Vulkan##T##Ref = Handle<Vulkan##T>;                                         \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;                                 \
                                                                                      \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>> \
    static inline Vulkan##T* VulkanCastImpl(Base* ptr)                                \
    {                                                                                 \
        return static_cast<Vulkan##T*>(ptr);                                          \
    }                                                                                 \
                                                                                      \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>> \
    static inline const Vulkan##T* VulkanCastImpl(const Base* ptr)                    \
    {                                                                                 \
        return static_cast<const Vulkan##T*>(ptr);                                    \
    }                                                                                 \
                                                                                      \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>> \
    static inline Vulkan##T##Ref VulkanCastImpl(const Handle<Base>& ref)              \
    {                                                                                 \
        return Vulkan##T##Ref(ref);                                                   \
    }                                                                                 \
                                                                                      \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>> \
    static inline Vulkan##T##WeakRef VulkanCastImpl(const WeakHandle<Base>& ref)      \
    {                                                                                 \
        return Vulkan##T##WeakRef(ref);                                               \
    }

#define DECLARE_VULKAN_GFX_TYPE(T)            \
    class Vulkan##T;                          \
                                              \
    using Vulkan##T##Ref = Handle<Vulkan##T>; \
    using Vulkan##T##WeakRef = WeakHandle<Vulkan##T>;

#define VULKAN_CAST(a) VulkanCastImpl(a)

/*! \brief Enqueues a render object to be created with the given args on the render thread, or creates it immediately if already on the render thread.
 *
 *  \param ref The render object to create.
 *  \param args The arguments to pass to the render object's constructor.
 */
template <class RefType, class... Args>
static inline void DeferCreate(RefType ref, Args&&... args)
{
    struct RENDER_COMMAND(CallCreateOnRenderThread) final : RenderCommand
    {
        using ArgsTuple = Tuple<std::decay_t<Args>...>;

        RefType ref;
        ArgsTuple args;

        RENDER_COMMAND(CallCreateOnRenderThread)(RefType&& ref, Args&&... args)
            : ref(std::move(ref)),
              args(std::forward<Args>(args)...)
        {
        }

        virtual ~RENDER_COMMAND(CallCreateOnRenderThread)() override = default;

        virtual RendererResult operator()() override
        {
            return Apply([this]<class... OtherArgs>(OtherArgs&&... args)
                {
                    return ref->Create(std::forward<OtherArgs>(args)...);
                },
                std::move(args));
        }
    };

    if (!ref.IsValid())
    {
        return;
    }

    PUSH_RENDER_COMMAND(CallCreateOnRenderThread, std::move(ref), std::forward<Args>(args)...);
}

#if HYP_VULKAN
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_VULKAN;                     \
    using T##Ref = T##Ref##_VULKAN;           \
    using T##WeakRef = T##WeakRef##_VULKAN
#elif HYP_WEBGPU
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_WEBGPU;                     \
    using T##Ref = T##Ref##_WEBGPU;           \
    using T##WeakRef = T##WeakRef##_WEBGPU
#endif

#include <rendering/inl/RenderObjectDefinitions.inl>

#undef DEF_RENDER_OBJECT
#undef DEF_RENDER_OBJECT_WITH_BASE_CLASS
#undef DEF_RENDER_OBJECT_NAMED
#undef DEF_RENDER_PLATFORM_OBJECT
#undef DEF_CURRENT_PLATFORM_RENDER_OBJECT

} // namespace hyperion
