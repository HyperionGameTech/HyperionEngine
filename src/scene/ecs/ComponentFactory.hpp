/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_FACTORY_HPP
#define HYPERION_ECS_COMPONENT_FACTORY_HPP

#include <core/utilities/TypeID.hpp>
#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class ComponentFactoryBase
{
public:
    virtual ~ComponentFactoryBase() = default;

    virtual UniquePtr<void> CreateComponent() = 0;
};

template <class Component>
class ComponentFactory : public ComponentFactoryBase
{
public:
    virtual ~ComponentFactory() = default;

    virtual UniquePtr<void> CreateComponent() override
    {
        return UniquePtr<Component>(new Component { });
    }
};

} // namespace hyperion

#endif