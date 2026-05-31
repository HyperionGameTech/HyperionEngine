/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/UniquePtr.hpp>

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
