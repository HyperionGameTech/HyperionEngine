/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Subsystem.hpp>

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
