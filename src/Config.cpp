#include <Config.hpp>
#include <Engine.hpp>

#include <rendering/backend/vulkan/RendererFeatures.hpp>

namespace hyperion::v2 {

Configuration::Configuration() = default;

void Configuration::SetToDefaultConfiguration(Engine *engine)
{
    m_variables = { };

    m_variables[CONFIG_RT_ENABLED] = engine->GetDevice()->GetFeatures().IsRaytracingEnabled();
    m_variables[CONFIG_RT_REFLECTIONS] = engine->GetDevice()->GetFeatures().IsRaytracingEnabled();
    m_variables[CONFIG_RT_GI] = engine->GetDevice()->GetFeatures().IsRaytracingEnabled();
    
    m_variables[CONFIG_SSR] &= ~m_variables[CONFIG_RT_REFLECTIONS];
    m_variables[CONFIG_VOXEL_GI] &= ~m_variables[CONFIG_RT_GI];
}

} // namespace hyperion::v2