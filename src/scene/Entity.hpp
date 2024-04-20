/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

namespace hyperion {

class HYP_API Entity : public BasicObject<STUB_CLASS(Entity)>
{
public:
    Entity();
    ~Entity();

    bool IsReady() const;
    void Init();
};

} // namespace hyperion

#endif
