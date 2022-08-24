#ifndef HYPERION_V2_SAFE_DELETER_H
#define HYPERION_V2_SAFE_DELETER_H

#include <core/Containers.hpp>
#include <core/lib/Pair.hpp>
#include <core/Handle.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Shader.hpp>
#include <animation/Skeleton.hpp>

#include <Types.hpp>

#include <mutex>
#include <tuple>
#include <atomic>

namespace hyperion::v2 {

class Engine;

using RenderableDeletionMaskBits = UInt;

enum RenderableDeletionMask : RenderableDeletionMaskBits
{
    RENDERABLE_DELETION_NONE      = 0,
    RENDERABLE_DELETION_TEXTURES  = 1 << 0,
    RENDERABLE_DELETION_MATERIALS = 1 << 1,
    RENDERABLE_DELETION_MESHES    = 1 << 2,
    RENDERABLE_DELETION_SKELETONS = 1 << 3,
    RENDERABLE_DELETION_SHADERS   = 1 << 4
};

class SafeDeleter
{
    static constexpr UInt8 initial_cycles_remaining = static_cast<UInt8>(max_frames_in_flight + 1);

public:
    void SafeReleaseRenderResource(Handle<Texture> &&resource)
    {
        SafeReleaseRenderResourceImpl<Texture>(std::move(resource), RENDERABLE_DELETION_TEXTURES);
    }
    
    void SafeReleaseRenderResource(Handle<Mesh> &&resource)
    {
        SafeReleaseRenderResourceImpl<Mesh>(std::move(resource), RENDERABLE_DELETION_MESHES);
    }
    
    void SafeReleaseRenderResource(Handle<Skeleton> &&resource)
    {
        SafeReleaseRenderResourceImpl<Skeleton>(std::move(resource), RENDERABLE_DELETION_SKELETONS);
    }
    
    void SafeReleaseRenderResource(Handle<Shader> &&resource)
    {
        SafeReleaseRenderResourceImpl<Shader>(std::move(resource), RENDERABLE_DELETION_SHADERS);
    }

    template <class T>
    struct RenderableDeletionEntry : public KeyValuePair<Handle<T>, UInt8>
    {
        using Base = KeyValuePair<Handle<T>, UInt8>;

        static_assert(std::is_base_of_v<RenderResource, T>, "T must be a derived class of RenderResource");

        RenderableDeletionEntry(Handle<T> &&renderable)
            : Base(std::move(renderable), initial_cycles_remaining)
        {
        }

        RenderableDeletionEntry(const RenderableDeletionEntry &other) = delete;
        RenderableDeletionEntry &operator=(const RenderableDeletionEntry &other) = delete;

        RenderableDeletionEntry(RenderableDeletionEntry &&other) noexcept
            : Base(std::move(other))
        {
        }

        RenderableDeletionEntry &operator=(RenderableDeletionEntry &&other) noexcept
        {
            Base::operator=(std::move(other));

            return *this;
        }

        ~RenderableDeletionEntry() = default;

        void PerformDeletion()
        {
            // cycle should be at zero
            AssertThrow(Base::second == 0u);

            Base::first.Reset();
        }
    };

    void PerformEnqueuedDeletions();

    // returns true if all were completed
    template <class T>
    bool DeleteEnqueuedItemsOfType()
    {
        auto &queue = std::get<FlatSet<RenderableDeletionEntry<T>>>(m_render_resource_deletion_queue_items);

        for (auto it = queue.Begin(); it != queue.End();) {
            RenderableDeletionEntry<T> &front = *it;

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

    template <class T>
    bool DeleteAllItemsOfType()
    {
        auto &queue = std::get<FlatSet<RenderableDeletionEntry<T>>>(m_render_resource_deletion_queue_items);

        for (auto it = queue.Begin(); it != queue.End();) {
            RenderableDeletionEntry<T> &front = *it;

            AssertThrow(front.first != nullptr);
            front.first.Reset();
            it = queue.Erase(it);
        }

        return queue.Empty();
    }

    void ForceReleaseAll();

private:
    template <class T>
    void SafeReleaseRenderResourceImpl(Handle<T> &&resource, UInt mask)
    {
        if (resource) {// && resource.GetRefCount() == 1) {
            std::lock_guard guard(m_render_resource_deletion_mutex);
            
            auto &deletion_queue = std::get<FlatSet<RenderableDeletionEntry<T>>>(m_render_resource_deletion_queue_items);
    
            deletion_queue.Insert(RenderableDeletionEntry<T>(std::move(resource)));

            m_render_resource_deletion_flag |= mask;
        }
    }

    std::tuple<
        FlatSet<RenderableDeletionEntry<Texture>>,
        FlatSet<RenderableDeletionEntry<Mesh>>,
        FlatSet<RenderableDeletionEntry<Skeleton>>,
        FlatSet<RenderableDeletionEntry<Shader>>
    > m_render_resource_deletion_queue_items;
    
    std::mutex m_render_resource_deletion_mutex;
    std::atomic<RenderableDeletionMaskBits> m_render_resource_deletion_flag { RENDERABLE_DELETION_NONE };
};

} // namespace hyperion::v2

#endif