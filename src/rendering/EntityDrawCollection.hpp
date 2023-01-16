#ifndef HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP

#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <util/Defines.hpp>

namespace hyperion::v2 {

class Scene;
class Camera;
class Entity;
class RenderGroup;

class EntityDrawCollection
{
    FlatMap<RenderableAttributeSet, Array<EntityDrawProxy>> m_entities;

public:
    using Iterator = FlatMap<RenderableAttributeSet, Array<EntityDrawProxy>>::Iterator;
    using ConstIterator = FlatMap<RenderableAttributeSet, Array<EntityDrawProxy>>::ConstIterator;

    void Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity);
    void Reset();

    HYP_DEF_STL_BEGIN_END(m_entities.Begin(), m_entities.End())
};

class RenderList
{
public:
    friend class Scene;

    void PushEntityToRender(
        const Handle<Camera> &camera,
        const Handle<Entity> &entity,
        const RenderableAttributeSet *override_attributes
    );

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
        Call after calling CollectEntities() on Scene. */
    void UpdateRenderGroups();

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

private:
    EntityDrawCollection m_draw_collection;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>> m_render_groups;
};

} // namespace hyperion::v2

#endif