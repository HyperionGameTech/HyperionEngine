/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_ENTITY_DRAW_COLLECTION_HPP

#include <core/containers/ArrayMap.hpp>
#include <core/threading/Threads.hpp>
#include <core/ID.hpp>

#include <math/Transform.hpp>

#include <rendering/DrawProxy.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResourceManager.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/CullData.hpp>
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

using renderer::Frame;

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
    Array<RenderProxy>                          render_proxies;
    Handle<RenderGroup>                         render_group;

    RenderProxyGroup() = default;
    RenderProxyGroup(const RenderProxyGroup &other);
    RenderProxyGroup &operator=(const RenderProxyGroup &other);
    RenderProxyGroup(RenderProxyGroup &&other) noexcept;
    RenderProxyGroup &operator=(RenderProxyGroup &&other) noexcept;
    ~RenderProxyGroup() = default;

    void ClearProxies()
    {
        render_proxies.Clear();

        // Do not clear render group; keep it reserved
    }
};

class EntityDrawCollection
{
public:
    void AddRenderProxy(ThreadType thread_type, const RenderableAttributeSet &attributes, const RenderProxy &proxy);
    void AddRenderProxy(const RenderableAttributeSet &attributes, const RenderProxy &proxy);

    void ClearProxyGroups();

    FixedArray<ArrayMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &GetProxyGroups();
    const FixedArray<ArrayMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &GetProxyGroups() const;

    RenderProxyList &GetProxyList(ThreadType);
    const RenderProxyList &GetProxyList(ThreadType) const;

private:
    FixedArray<ArrayMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX>                                       m_proxy_groups;
    RenderResourceManager                                                                                               m_current_render_side_resources;


    FixedArray<RenderProxyList, ThreadType::THREAD_TYPE_MAX>                                                            m_proxy_lists;
};

struct PushConstantData
{
    const void *ptr = nullptr;
    SizeType size = 0;

    PushConstantData()
        : ptr(nullptr),
          size(0)
    {
    }

    template <class T>
    PushConstantData(const T *value)
    {
        static_assert(sizeof(T) <= 128, "sizeof(T) must be <= 128");

        ptr = value;
        size = sizeof(T);
    }

    PushConstantData(const PushConstantData &other)                 = default;
    PushConstantData &operator=(const PushConstantData &other)      = default;
    PushConstantData(PushConstantData &&other) noexcept             = default;
    PushConstantData &operator=(PushConstantData &&other) noexcept  = default;
    ~PushConstantData()                                             = default;

    explicit operator bool() const
        { return ptr && size; }
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
    void UpdateOnRenderThread(
        const Handle<Framebuffer> &framebuffer = Handle<Framebuffer>::empty,
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
        const Handle<Framebuffer> &framebuffer,
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
        const Handle<Framebuffer> &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCallsInLayers(
        Frame *frame,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCallsInLayers(
        Frame *frame,
        const Handle<Framebuffer> &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCallsInLayers(
        Frame *frame,
        const Handle<Camera> &camera,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCallsInLayers(
        Frame *frame,
        const Handle<Camera> &camera,
        const Handle<Framebuffer> &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

private:
    Handle<Camera>                                              m_camera;
    RC<EntityDrawCollection>                                    m_draw_collection;
};

} // namespace hyperion

#endif