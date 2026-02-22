/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/memory/UniquePtr.hpp>

namespace Hyperion {

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

} // namespace Hyperion
