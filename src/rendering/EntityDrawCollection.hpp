#ifndef HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP
#define HYPERION_V2_ENTITY_DRAW_COLLECTION_HPP

#include <core/Containers.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <util/Defines.hpp>

namespace hyperion::v2 {

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

} // namespace hyperion::v2

#endif