/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

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
