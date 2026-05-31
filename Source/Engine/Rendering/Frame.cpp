/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/Frame.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderConfig.hpp>

#include <Frame.generated.inl>

namespace Hyperion {

void FrameBase::OnFrameStart()
{
    m_frameCounter = GetFrameCounter();
}

} // namespace Hyperion
