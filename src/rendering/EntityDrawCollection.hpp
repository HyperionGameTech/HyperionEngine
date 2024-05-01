/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_ENTITY_DRAW_COLLECTION_HPP

#include <core/containers/ArrayMap.hpp>
#include <core/threading/Threads.hpp>
#include <core/ID.hpp>

#include <math/Transform.hpp>

#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawData.hpp>
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

struct EntityList
{
    Array<EntityDrawData>   entity_draw_datas;
    Handle<RenderGroup>     render_group;
    RenderResourceManager   resources;

    EntityList() = default;
    EntityList(const EntityList &other)             = delete;
    EntityList &operator=(const EntityList &other)  = delete;
    EntityList(EntityList &&other) noexcept;
    EntityList &operator=(EntityList &&other) noexcept;
    ~EntityList() = default;

    void ClearEntities()
    {
        entity_draw_datas.Clear();

        // Reset resource usage
        resources.Reset();

        // Do not clear render group; keep it reserved
    }
};

class EntityDrawCollection
{
public:
    void InsertEntityWithAttributes(const RenderableAttributeSet &attributes, const EntityDrawData &entity_draw_data);

    void ClearEntities();

    void SetRenderSideList(const RenderableAttributeSet &attributes, EntityList &&entity_list);

    /*! \brief Update the render thread side resource manager using another ResourceManager
     *  that was handed off from the game thread. \ref{resources} only needs to hold Handle<T> objects
     *  for newly added resources, as they will be taken and stored in the render side resource manager. */
    void UpdateRenderSideResources(RenderResourceManager &resources);

    FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList();
    const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList() const;

    FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList(ThreadType);
    const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList(ThreadType) const;

    HashCode CalculateCombinedAttributesHashCode() const;

private:
    FixedArray<FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX>, ThreadType::THREAD_TYPE_MAX>    m_lists;
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

    void ClearEntities();

    /*! \brief Pushes an Entity to the RenderList.
     *  \param camera The camera to use for rendering.
     *  \param entity_id The ID of the Entity.
     *  \param mesh The Mesh to render.
     *  \param material The Material to use for rendering.
     *  \param skeleton The Skeleton to use for rendering.
     *  \param model_matrix The model matrix of the Entity.
     *  \param previous_model_matrix The previous model matrix of the Entity.
     *  \param aabb The AABB of the Entity.
     *  \param override_attributes The RenderableAttributeSet to use for rendering.
     */
    void PushEntityToRender(
        const Handle<Camera> &camera,
        ID<Entity> entity_id,
        const Handle<Mesh> &mesh,
        const Handle<Material> &material,
        const Handle<Skeleton> &skeleton,
        const Matrix4 &model_matrix,
        const Matrix4 &previous_model_matrix,
        const BoundingBox &aabb,
        const RenderableAttributeSet *override_attributes
    );

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
     *  Call after calling CollectEntities() on Scene. */
    void UpdateRenderGroups();

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
    HashMap<RenderableAttributeSet, WeakHandle<RenderGroup>>    m_render_groups;

    // Keeps track of resources that are used per frame.
    // Handles to the objects are not persistently held, as they are moved when passed to the render command UpdateRenderSideResources
    RenderResourceManager                                       m_resources;
};

} // namespace hyperion

#endif