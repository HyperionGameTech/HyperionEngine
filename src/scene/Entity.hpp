/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ENTITY_HPP
#define HYPERION_V2_ENTITY_HPP

#include <core/Base.hpp>

namespace hyperion::v2 {

class HYP_API Entity : public BasicObject<STUB_CLASS(Entity)>
{
public:
    Entity();
    ~Entity();

    bool IsReady() const;
    void Init();
};

} // namespace hyperion::v2

#endif
