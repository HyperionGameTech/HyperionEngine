#ifndef HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP

#include <core/lib/ArrayMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/ID.hpp>
#include <math/Transform.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawData.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/CullData.hpp>
#include <util/Defines.hpp>
#include <Threads.hpp>
#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {
template <PlatformType PLATFORM>
class Frame;
} // namespace platform

using Frame = platform::Frame<Platform::CURRENT>;
} // namespace hyperion::renderer

namespace hyperion::v2 {

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

class EntityDrawCollection
{
public:
    struct EntityList
    {
        Array<EntityDrawData>   entity_draw_datas;
        Handle<RenderGroup>     render_group;
    };

    void Insert(const RenderableAttributeSet &attributes, EntityDrawData entity_draw_data);
    void ClearEntities();

    void SetRenderSideList(const RenderableAttributeSet &attributes, EntityList &&entity_list);

    FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList();
    const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList() const;

    FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList(ThreadType);
    const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &GetEntityList(ThreadType) const;

    HashCode CalculateCombinedAttributesHashCode() const;

private:
    FixedArray<FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX>, THREAD_TYPE_MAX>    m_lists;
    FixedArray<ArrayMap<RenderableAttributeSet, RenderResourceManager>, PASS_TYPE_MAX>                      m_render_side_resources;
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

    PushConstantData(const PushConstantData &other) = default;
    PushConstantData &operator=(const PushConstantData &other) = default;
    PushConstantData(PushConstantData &&other) noexcept = default;
    PushConstantData &operator=(PushConstantData &&other) noexcept = default;
    ~PushConstantData() = default;

    explicit operator bool() const
        { return ptr && size; }
};

struct RenderListQuery
{
    Bucket bucket = BUCKET_INVALID;

    explicit operator bool() const
        { return bucket != BUCKET_INVALID; }
};

class RenderList
{
public:
    RenderList();
    RenderList(const Handle<Camera> &camera);
    RenderList(const RenderList &other) = default;
    RenderList &operator=(const RenderList &other) = default;
    RenderList(RenderList &&other) noexcept = default;
    RenderList &operator=(RenderList &&other) noexcept = default;
    ~RenderList() = default;

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    void SetCamera(const Handle<Camera> &camera)
        { m_camera = camera; }

    const RC<EntityDrawCollection> &GetEntityCollection() const
        { return m_draw_collection; }

    void ClearEntities();

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
        Call after calling CollectEntities() on Scene. */
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

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

private:
    Handle<Camera>                                          m_camera;
    RC<EntityDrawCollection>                                m_draw_collection;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>>    m_render_groups;
};

} // namespace hyperion::v2

#endif