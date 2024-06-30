/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Threads.hpp>
#include <core/ID.hpp>

#include <math/Transform.hpp>

#include <rendering/DrawProxy.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/CullData.hpp>

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

using renderer::PushConstantData;

enum PassType : uint
{
    PASS_TYPE_INVALID = uint(-1),
    PASS_TYPE_SKYBOX = 0,
    PASS_TYPE_OPAQUE,
    PASS_TYPE_TRANSLUCENT,
    PASS_TYPE_UI,
    PASS_TYPE_MAX
};

constexpr PassType BucketToPassType(Bucket bucket)
{
    constexpr const PassType pass_type_per_bucket[uint(BUCKET_MAX)] = {
        PASS_TYPE_INVALID,     // BUCKET_SWAPCHAIN
        PASS_TYPE_INVALID,     // BUCKET_RESERVED0
        PASS_TYPE_INVALID,     // BUCKET_SHADOW
        PASS_TYPE_OPAQUE,      // BUCKET_OPAQUE
        PASS_TYPE_TRANSLUCENT, // BUCKET_TRANSLUCENT
        PASS_TYPE_SKYBOX,      // BUCKET_SKYBOX
        PASS_TYPE_UI           // BUCKET_UI
    };

    return pass_type_per_bucket[uint(bucket)];
}

struct RenderProxyGroup
{
    Array<RenderProxy>  m_render_proxies;
    Handle<RenderGroup> m_render_group;

public:
    RenderProxyGroup() = default;
    RenderProxyGroup(const RenderProxyGroup &other);
    RenderProxyGroup &operator=(const RenderProxyGroup &other);
    RenderProxyGroup(RenderProxyGroup &&other) noexcept;
    RenderProxyGroup &operator=(RenderProxyGroup &&other) noexcept;
    ~RenderProxyGroup() = default;

    void ClearProxies();

    void AddRenderProxy(const RenderProxy &render_proxy);
    bool RemoveRenderProxy(ID<Entity> entity);

    const Array<RenderProxy> &GetRenderProxies() const
        { return m_render_proxies; }

    void ResetRenderGroup();

    void SetRenderGroup(const Handle<RenderGroup> &render_group);

    const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }
};

class EntityDrawCollection
{
public:
    void ClearProxyGroups(bool reset_render_groups = false);
    void RemoveEmptyProxyGroups();

    FixedArray<FlatMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &GetProxyGroups();
    const FixedArray<FlatMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &GetProxyGroups() const;

    RenderProxyList &GetProxyList(ThreadType);
    const RenderProxyList &GetProxyList(ThreadType) const;

private:
    FixedArray<FlatMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX>    m_proxy_groups;
    FixedArray<RenderProxyList, ThreadType::THREAD_TYPE_MAX>                        m_proxy_lists;
};

struct RenderListCollectionResult
{
    uint32  num_added_entities = 0;
    uint32  num_removed_entities = 0;
    uint32  num_changed_entities = 0;

    /*! \brief Returns true if any proxies have been added, removed or changed. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool NeedsUpdate() const
        { return num_added_entities != 0 || num_removed_entities != 0 || num_changed_entities != 0; }
};

class RenderList
{
public:
    RenderList();
    RenderList(const Handle<Camera> &camera);
    RenderList(const RenderList &other)                 = delete;
    RenderList &operator=(const RenderList &other)      = delete;
    RenderList(RenderList &&other) noexcept             = default;
    RenderList &operator=(RenderList &&other) noexcept  = default;
    ~RenderList();

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    void SetCamera(const Handle<Camera> &camera)
        { m_camera = camera; }

    const RC<EntityDrawCollection> &GetEntityCollection() const
        { return m_draw_collection; }

    /*! \brief Pushes an entity to the RenderList.
     *  \param entity The entity the proxy is used for
     *  \param proxy A RenderProxy associated with the entity
     */
    void PushEntityToRender(
        ID<Entity> entity,
        const RenderProxy &proxy
    );

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
     *  Call after calling CollectEntities() on Scene. */
    RenderListCollectionResult PushUpdatesToRenderThread(
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
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        Frame *frame,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        Frame *frame,
        const Handle<Camera> &camera,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        Frame *frame,
        const Handle<Camera> &camera,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

protected:
    Handle<Camera>                                              m_camera;
    RC<EntityDrawCollection>                                    m_draw_collection;
};

} // namespace hyperion

#endif