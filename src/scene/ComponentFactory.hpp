/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>

#include <core/memory/UniquePtr.hpp>

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
