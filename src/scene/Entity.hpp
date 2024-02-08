#ifndef HYPERION_V2_ENTITY_HPP
#define HYPERION_V2_ENTITY_HPP

#include <core/Base.hpp>

namespace hyperion::v2 {

class Entity : public BasicObject<STUB_CLASS(Entity)>
{
public:
    Entity();
    ~Entity() = default;

    Bool IsReady() const;
    void Init();
};

} // namespace hyperion::v2

#endif
