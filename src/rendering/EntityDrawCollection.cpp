#include <rendering/EntityDrawCollection.hpp>

namespace hyperion::v2 {

void EntityDrawCollection::Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity)
{
    m_entities[attributes].PushBack(entity);
}

void EntityDrawCollection::Reset()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot.
    for (auto &it : m_entities) {
        it.second.Clear();
    }
}

} // namespace hyperion::v2