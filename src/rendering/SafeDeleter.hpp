#ifndef HYPERION_V2_SAFE_DELETER_H
#define HYPERION_V2_SAFE_DELETER_H

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/lib/Mutex.hpp>
#include <core/ID.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Shader.hpp>
#include <scene/animation/Skeleton.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <mutex>
#include <tuple>
#include <atomic>

namespace hyperion::v2 {

using renderer::Device;
using renderer::GPUImageMemory;

using HandleDeletionMask = UInt32;

enum HandleDeletionMaskBits : HandleDeletionMask
{
    RENDERABLE_DELETION_NONE              = 0,
    RENDERABLE_DELETION_BUFFERS_OR_IMAGES = 1 << 0,
    RENDERABLE_DELETION_TEXTURES          = 1 << 1,
    RENDERABLE_DELETION_MATERIALS         = 1 << 2,
    RENDERABLE_DELETION_MESHES            = 1 << 3,
    RENDERABLE_DELETION_SKELETONS         = 1 << 4,
    RENDERABLE_DELETION_SHADERS           = 1 << 5
};

class SafeDeleter
{

public:
    static constexpr UInt8 initial_cycles_remaining = UInt8(max_frames_in_flight + 1);

    void SafeReleaseHandle(Handle<Texture> &&resource)
    {
        SafeReleaseHandleImpl<Texture>(std::move(resource), RENDERABLE_DELETION_TEXTURES);
    }
    
    void SafeReleaseHandle(Handle<Material> &&resource)
    {
        SafeReleaseHandleImpl<Material>(std::move(resource), RENDERABLE_DELETION_MATERIALS);
    }
    
    void SafeReleaseHandle(Handle<Mesh> &&resource)
    {
        SafeReleaseHandleImpl<Mesh>(std::move(resource), RENDERABLE_DELETION_MESHES);
    }
    
    void SafeReleaseHandle(Handle<Skeleton> &&resource)
    {
        SafeReleaseHandleImpl<Skeleton>(std::move(resource), RENDERABLE_DELETION_SKELETONS);
    }
    
    void SafeReleaseHandle(Handle<Shader> &&resource)
    {
        SafeReleaseHandleImpl<Shader>(std::move(resource), RENDERABLE_DELETION_SHADERS);
    }

    /**
     * \brief Enqueue deletion of an object (buffer or image)
    */
    template <class T>
    void SafeRelease(UniquePtr<T> &&resource)
    {
        // static_assert(std::is_base_of_v<GPUBuffer, T> || std::is_base_of_v<GPUImageMemory, T>,
        //     "Must be a class of GPUBuffer or GPUImageMemory");

        if (resource) {
            Mutex::Guard guard(m_render_resource_deletion_mutex);
            
            m_buffers_and_images.Insert(BufferOrImageDeletionEntry(std::move(resource)));

            m_render_resource_deletion_flag.BitOr(RENDERABLE_DELETION_BUFFERS_OR_IMAGES, MemoryOrder::ACQUIRE_RELEASE);
        }
    }
    
    template <class T>
    struct HandleDeletionEntry : private KeyValuePair<Handle<T>, UInt8>
    {
        using Base = KeyValuePair<Handle<T>, UInt8>;

        using Base::first;
        using Base::second;

        HandleDeletionEntry(Handle<T> &&handle)
            : Base(std::move(handle), initial_cycles_remaining)
        {
        }

        HandleDeletionEntry(const Handle<T> &handle) = delete;

        HandleDeletionEntry(const HandleDeletionEntry &other) = delete;
        HandleDeletionEntry &operator=(const HandleDeletionEntry &other) = delete;

        HandleDeletionEntry(HandleDeletionEntry &&other) noexcept
            : Base(std::move(other))
        {
        }

        HandleDeletionEntry &operator=(HandleDeletionEntry &&other) noexcept
        {
            Base::operator=(std::move(other));

            return *this;
        }

        ~HandleDeletionEntry() = default;

        Bool operator==(const HandleDeletionEntry &other) const
            { return Base::operator==(other); }

        Bool operator<(const HandleDeletionEntry &other) const
            { return Base::operator<(other); }
    
        void PerformDeletion(bool force = false)
        {
            // cycle should be at zero
            AssertThrow(force || Base::second == 0u);

            Base::first.Reset();
        }
    };

    // stores as UniquePtr<void> (type erasure) and a function pointer to destroy it
    struct BufferOrImageDeletionEntry : public KeyValuePair<UniquePtr<void>, UInt8>
    {
        using Base = KeyValuePair<UniquePtr<void>, UInt8>;

        renderer::Result (*destroy_buffer_fn)(void *ptr, Device *device);

        template <class T>
        BufferOrImageDeletionEntry(UniquePtr<T> &&ptr)
            : Base(ptr.template Cast<void>(), initial_cycles_remaining),
              destroy_buffer_fn([](void *ptr, Device *device) {
                  auto *t_ptr = static_cast<T *>(ptr);
                  AssertThrow(t_ptr != nullptr);

                  return t_ptr->Destroy(device);
              })
        {
        }

        BufferOrImageDeletionEntry(const BufferOrImageDeletionEntry &other) = delete;
        BufferOrImageDeletionEntry &operator=(const BufferOrImageDeletionEntry &other) = delete;

        BufferOrImageDeletionEntry(BufferOrImageDeletionEntry &&other) noexcept
            : Base(std::move(other)),
              destroy_buffer_fn(other.destroy_buffer_fn)
        {
            other.destroy_buffer_fn = nullptr;
        }

        BufferOrImageDeletionEntry &operator=(BufferOrImageDeletionEntry &&other) noexcept
        {
            Base::operator=(std::move(other));

            destroy_buffer_fn = other.destroy_buffer_fn;
            other.destroy_buffer_fn = nullptr;

            return *this;
        }

        ~BufferOrImageDeletionEntry() = default;

        void PerformDeletion(Bool force = false)
        {
            // cycle should be at zero
            AssertThrow(force || Base::second == 0u);

            if (Base::first != nullptr) {
                AssertThrow(destroy_buffer_fn != nullptr);
                auto result = destroy_buffer_fn(Base::first.Get(), GetEngineDevice());

                if (!result) {
                    DebugLog(
                        LogType::Error,
                        "While deleting buffer or image, got error: %s [%d]\n",
                        result.message,
                        result.error_code
                    );
                }

                // if there was an error previously, a crash here may happen.
                Base::first.Reset();
            }

            destroy_buffer_fn = nullptr;
        }
    };

    void PerformEnqueuedDeletions();
    void ForceReleaseAll();

    bool DeleteEnqueuedBuffersAndImages()
    {
        auto &queue = m_buffers_and_images;

        return DeleteEnqueuedItemsImpl(queue);
    }

    void ForceDeleteBuffersAndImages()
    {
        auto &queue = m_buffers_and_images;

        ForceDeleteEnqueuedItemsImpl(queue);
    }

    template <class T>
    bool DeleteEnqueuedHandlesOfType()
    {
        auto &queue = std::get<FlatSet<HandleDeletionEntry<T>>>(m_render_resource_deletion_queue_items);

        return DeleteEnqueuedItemsImpl(queue);
    }

    template <class T>
    void ForceDeleteHandlesOfType()
    {
        auto &queue = std::get<FlatSet<HandleDeletionEntry<T>>>(m_render_resource_deletion_queue_items);

        ForceDeleteEnqueuedItemsImpl(queue);
    }

    // returns true if all were completed
    template <class Container>
    bool DeleteEnqueuedItemsImpl(Container &queue)
    {
        for (auto it = queue.Begin(); it != queue.End();) {
            auto &front = *it;

            AssertThrow(front.first != nullptr);

            if (!front.second) {
                front.PerformDeletion();

                it = queue.Erase(it);
            } else {
                --front.second;

                ++it;
            }
        }

        return queue.Empty();
    }

    template <class Container>
    void ForceDeleteEnqueuedItemsImpl(Container &queue)
    {
        for (auto it = queue.Begin(); it != queue.End();) {
            auto &front = *it;

            AssertThrow(front.first != nullptr);
            front.PerformDeletion(true);
            it = queue.Erase(it);
        }

        AssertThrow(queue.Empty());
    }

private:
    template <class T>
    void SafeReleaseHandleImpl(Handle<T> &&resource, UInt32 mask)
    {
        if (!resource) {
            return;
        }

        Mutex::Guard guard(m_render_resource_deletion_mutex);
        
        auto &deletion_queue = std::get<FlatSet<HandleDeletionEntry<T>>>(m_render_resource_deletion_queue_items);
        
        deletion_queue.Insert(HandleDeletionEntry<T> { std::move(resource) });

        m_render_resource_deletion_flag.BitOr(mask, MemoryOrder::ACQUIRE_RELEASE);
    }

    void EnqueueTextureBindlessStorageRemoval(ID<Texture> id);

    std::tuple<
        FlatSet<HandleDeletionEntry<Texture>>,
        FlatSet<HandleDeletionEntry<Mesh>>,
        FlatSet<HandleDeletionEntry<Material>>,
        FlatSet<HandleDeletionEntry<Skeleton>>,
        FlatSet<HandleDeletionEntry<Shader>>
    > m_render_resource_deletion_queue_items;

    FlatSet<BufferOrImageDeletionEntry> m_buffers_and_images;
    
    Mutex                               m_render_resource_deletion_mutex;
    AtomicVar<HandleDeletionMask>       m_render_resource_deletion_flag { RENDERABLE_DELETION_NONE };
};

} // namespace hyperion::v2

#endif