/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_FACTORY_HPP
#define HYPERION_ECS_COMPONENT_FACTORY_HPP

#include <core/utilities/TypeID.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/object/HypData.hpp>

namespace hyperion {

class IComponentFactory
{
public:
    virtual ~IComponentFactory() = default;
};

template <class Component>
class ComponentFactory : public IComponentFactory
{
public:
    virtual ~ComponentFactory() override = default;
};

} // namespace hyperion

#endif