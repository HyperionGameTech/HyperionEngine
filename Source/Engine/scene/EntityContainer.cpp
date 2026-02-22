/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/EntityContainer.hpp>

namespace Hyperion {

EntityContainer& EntityContainer::GetDefaultInstance()
{
    static EntityContainer s_defaultInstance;
    return s_defaultInstance;
}

} // namespace Hyperion
