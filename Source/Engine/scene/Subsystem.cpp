/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Subsystem.hpp>

#include <Subsystem.generated.inl>

namespace Hyperion {

Subsystem::Subsystem()
    : m_updatePhase(SubsystemUpdatePhase::BeforeVis)
{
}

Subsystem::~Subsystem()
{
}

} // namespace Hyperion
