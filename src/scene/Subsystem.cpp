/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Subsystem.hpp>

#include <Subsystem.generated.inl>

namespace hyperion {

Subsystem::Subsystem()
    : m_updatePhase(SubsystemUpdatePhase::BeforeVis)
{
}

Subsystem::~Subsystem()
{
}

} // namespace hyperion
