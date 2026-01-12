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

/*! \brief Enqueues a render object to be created with the given args on the render thread, or creates it immediately if already on the render thread.
 *
 *  \param ref The render object to create.
 *  \param args The arguments to pass to the render object's constructor.
 */
template <class RefType, class... Args>
static inline void DeferCreate(RefType ref, Args&&... args)
{
    struct CallCreateOnRenderThread final : RenderCommand
    {
        using ArgsTuple = Tuple<std::decay_t<Args>...>;

        RefType ref;
        ArgsTuple args;

        CallCreateOnRenderThread(RefType&& ref, Args&&... args)
            : ref(std::move(ref)),
              args(std::forward<Args>(args)...)
        {
        }

        virtual ~CallCreateOnRenderThread() override = default;

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

#include <rendering/inl/RenderObjectDefinitions.inl>

struct ShaderUniform
{
    StringHash name;

    union
    {
        GpuBuffer* buffer;
        GpuImageView* imageView;
        Sampler* sampler;
        GpuTlas* tlas;
    };

    enum
    {
        UT_Buffer,
        UT_ImageView,
        UT_Sampler,
        UT_Tlas
    } type;

    HYP_FORCE_INLINE bool operator==(const ShaderUniform& other) const
    {
        return Memory::MemCmp(this, &other, sizeof(ShaderUniform)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderUniform& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(this),
            reinterpret_cast<const ubyte*>(this) + sizeof(ShaderUniform));
    }
};

} // namespace Hyperion
