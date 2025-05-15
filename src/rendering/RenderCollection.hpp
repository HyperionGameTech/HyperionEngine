/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/threading/DataRaceDetector.hpp>

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

namespace hyperion {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class CameraRenderResource;
class ViewRenderResource;

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

    HYP_FORCE_INLINE const RC<EntityDrawCollection> &GetDrawCollection() const
        { return m_draw_collection; }

    HYP_FORCE_INLINE const Optional<RenderableAttributeSet> &GetOverrideAttributes() const
        { return m_override_attributes; }

    HYP_FORCE_INLINE void SetOverrideAttributes(const Optional<RenderableAttributeSet> &override_attributes)
        { m_override_attributes = override_attributes; }

    /*! \brief Pushes an RenderProxy to the RenderCollector.
        \param proxy_list The RenderProxyList to push to.
     *  \param proxy The RenderProxy to push.
     */
    void PushRenderProxy(RenderProxyList &proxy_list, const RenderProxy &render_proxy);

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
     *  Call after calling CollectEntities() on Scene. */
    CollectionResult PushUpdatesToRenderThread(const Handle<Camera> &camera, ViewRenderResource *view_render_resource);

    void CollectDrawCalls(
        FrameBase *frame,
        ViewRenderResource *view,
        const Bitset &bucket_bits,
        const CullData *cull_data
    );

    void ExecuteDrawCalls(
        FrameBase *frame,
        ViewRenderResource *view,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        FrameBase *frame,
        ViewRenderResource *view,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    /*! \brief Perform a full reset, when this is not needed anymore. Not thread safe, so ensure there will be no overlap of usage of this object when calling this. */
    void ClearState(bool create_new = true);

protected:

    RC<EntityDrawCollection>            m_draw_collection;
    Optional<RenderableAttributeSet>    m_override_attributes;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif