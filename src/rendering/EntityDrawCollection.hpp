#ifndef HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP

#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::renderer {

class Frame;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class World;

class EntityDrawCollection
{
public:
    friend class World;

    enum ThreadType : UInt
    {
        THREAD_TYPE_INVALID = UInt(-1),
        THREAD_TYPE_GAME = 0,
        THREAD_TYPE_RENDER = 1,
        THREAD_TYPE_MAX
    };

    struct EntityList
    {
        Array<EntityDrawProxy> drawables;
        Handle<RenderGroup> render_group;
    };

    void Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity);
    void ClearEntities();

    void SetEntityList(const RenderableAttributeSet &attributes, EntityList &&entities);

    FlatMap<RenderableAttributeSet, EntityList> &GetEntityList();
    FlatMap<RenderableAttributeSet, EntityList> &GetEntityList(ThreadType);

private:
    static ThreadType GetThreadType();

    FixedArray<FlatMap<RenderableAttributeSet, EntityList>, THREAD_TYPE_MAX> m_entities;
};

class RenderList
{
public:
    friend class World;

    RenderList();
    RenderList(const RenderList &other) = default;
    RenderList &operator=(const RenderList &other) = default;
    RenderList(RenderList &&other) noexcept = default;
    RenderList &operator=(RenderList &&other) noexcept = default;
    ~RenderList() = default;

    void ClearEntities();

    void PushEntityToRender(
        const Handle<Camera> &camera,
        const Handle<Entity> &entity,
        const RenderableAttributeSet *override_attributes
    );

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
        Call after calling CollectEntities() on Scene. */
    void UpdateRenderGroups();

    void Render(
        Frame *frame,
        const Scene *scene,
        const Handle<Camera> &camera,
        const void *push_constant_ptr = nullptr,
        SizeType push_constant_size = 0
    );

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

private:
    RC<EntityDrawCollection> m_draw_collection;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>> m_render_groups;
};

} // namespace hyperion::v2

#endif