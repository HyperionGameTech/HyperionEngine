/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/ID.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/CullData.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Frame;

} // namespace platform

using Frame = platform::Frame<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class CameraRenderResource;

using renderer::PushConstantData;

class EntityDrawCollection
{
public:
    void ClearProxyGroups();
    void RemoveEmptyProxyGroups();

    /*! \note To be used from render thread only */
    HYP_FORCE_INLINE FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX> &GetProxyGroups()
        { return m_proxy_groups; }

    /*! \note To be used from render thread only */
    HYP_FORCE_INLINE const FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX> &GetProxyGroups() const
        { return m_proxy_groups; }

    HYP_FORCE_INLINE RenderProxyList &GetProxyList(ThreadType thread_type)
        { return m_proxy_lists[uint32(thread_type)]; }

    HYP_FORCE_INLINE const RenderProxyList &GetProxyList(ThreadType thread_type) const
        { return m_proxy_lists[uint32(thread_type)]; }

    uint32 NumRenderGroups() const;

private:
    FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, Bucket::BUCKET_MAX>    m_proxy_groups;
    FixedArray<RenderProxyList, 2>                                                          m_proxy_lists;
};

class RenderCollector
{
public:
    struct CollectionResult
    {
        uint32  num_added_entities = 0;
        uint32  num_removed_entities = 0;
        uint32  num_changed_entities = 0;

        /*! \brief Returns true if any proxies have been added, removed or changed. */
        HYP_FORCE_INLINE bool NeedsUpdate() const
            { return num_added_entities != 0 || num_removed_entities != 0 || num_changed_entities != 0; }
    };

    RenderCollector();
    RenderCollector(const RenderCollector &other)                 = delete;
    RenderCollector &operator=(const RenderCollector &other)      = delete;
    RenderCollector(RenderCollector &&other) noexcept             = default;
    RenderCollector &operator=(RenderCollector &&other) noexcept  = default;
    ~RenderCollector();

    /*! \brief Pushes an entity to the RenderCollector.
     *  \param entity The entity the proxy is used for
     *  \param proxy A RenderProxy associated with the entity
     */
    void PushEntityToRender(ID<Entity> entity, const RenderProxy &proxy);

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
     *  Call after calling CollectEntities() on Scene. */
    CollectionResult PushUpdatesToRenderThread(
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    );

    void CollectDrawCalls(
        Frame *frame,
        const Bitset &bucket_bits,
        const CullData *cull_data
    );

    void ExecuteDrawCalls(
        Frame *frame,
        const CameraRenderResource &camera_render_resource,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        Frame *frame,
        const CameraRenderResource &camera_render_resource,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

protected:
    RC<EntityDrawCollection>        m_draw_collection;
};

} // namespace hyperion

#endif