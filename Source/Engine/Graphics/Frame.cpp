/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/Frame.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>

#include <Frame.generated.inl>

namespace Hyperion {

void FrameBase::OnFrameStart()
{
    m_frameCounter = GetFrameCounter();
}

} // namespace Hyperion
