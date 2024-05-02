/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_PROXY_HPP
#define HYPERION_RENDER_PROXY_HPP

#include <core/ID.hpp>
#include <core/utilities/UserData.hpp>
#include <core/containers/Bitset.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/Matrix4.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/animation/Skeleton.hpp>

namespace hyperion {

class Entity;

struct RenderProxy
{
    ID<Entity>          entity;
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Handle<Skeleton>    skeleton;
    Matrix4             model_matrix;
    Matrix4             previous_model_matrix;
    BoundingBox         aabb;
};

/*! \brief The action to take on call to \ref{RenderProxyList::Advance}. */
enum RenderProxyListAdvanceAction : uint32
{
    RPLAA_CLEAR,    //! Clear the 'next' entities, so on next iteration, any entities that have not been re-added are marked for removal.
    RPLAA_PERSIST,  //! Copy the previous entities over to next. To remove entities, `RemoveProxy` needs to be manually called.
};

class RenderProxyList
{
public:
    void Add(ID<Entity> entity, const RenderProxy &proxy);

    /*! \brief Mark to keep a proxy from the previous iteration for this iteration.
     *  Usable when \ref{Advance} is called with \ref{RPLAA_CLEAR}. Returns true if the
     *  proxy was successfully taken from the previous iteration, false otherwise.
     *  \param entity The entity to keep the proxy from the previous iteration for
     *  \returns Whether or not the proxy could be taken from the previous iteration */
    bool MarkToKeep(ID<Entity> entity);

    /*! \brief Mark the proxy to be removed in the next call to \ref{Advance} */
    void MarkToRemove(ID<Entity> entity);

    void GetRemovedEntities(Array<ID<Entity>> &out_entities);
    void GetAddedEntities(Array<RenderProxy *> &out_entities, bool include_changed = false);

    RenderProxy *GetProxyForEntity(ID<Entity> entity);

    const RenderProxy *GetProxyForEntity(ID<Entity> entity) const
        { return const_cast<RenderProxyList *>(this)->GetProxyForEntity(entity); }
        
    void Advance(RenderProxyListAdvanceAction action);

    /*! \brief Checks if the RenderProxyList already has a proxy for the given entity
     *  from the previous frame */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool HasProxyForEntity(ID<Entity> entity) const
        { return m_previous_entities.Test(entity.ToIndex()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Bitset &GetEntities() const
        { return m_previous_entities; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Bitset GetAddedEntities() const
        { return m_next_entities & ~m_previous_entities; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Bitset GetRemovedEntities() const
        { return m_previous_entities & ~m_next_entities; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Bitset &GetChangedEntities() const
        { return m_changed_entities; }


private:
    // @TODO use sparse array
    HashMap<ID<Entity>, RenderProxy>    m_proxies;

    Bitset                              m_previous_entities;
    Bitset                              m_next_entities;
    Bitset                              m_changed_entities;
};

} // namespace hyperion

#endif