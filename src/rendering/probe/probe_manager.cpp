#include "probe_manager.h"
#include "../renderer.h"
#include "../shader_manager.h"

namespace hyperion {
const int ProbeManager::voxel_map_size = 128;
const float ProbeManager::voxel_map_scale = 1.0f;
const int ProbeManager::voxel_map_num_mipmaps = 7;

ProbeManager *ProbeManager::instance = nullptr;

ProbeManager *ProbeManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ProbeManager();
    }

    return instance;
}

ProbeManager::ProbeManager()
    : m_spherical_harmonics_enabled(false),
      m_env_map_enabled(false),
      m_vct_enabled(false)
{
    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties()
            .Define("VCT_MAP_SIZE", voxel_map_size)
            .Define("VCT_NUM_MAPS", 6)
            .Define("VCT_SCALE", voxel_map_scale)
            .Define("VCT_GEOMETRY_SHADER", false)
            .Define("PROBE_RENDER_TEXTURES", true)
            .Define("PROBE_RENDER_SHADING", true)
            .Define("SPHERICAL_HARMONICS_ENABLED", m_spherical_harmonics_enabled)
            .Define("PROBE_ENABLED", m_env_map_enabled)
            .Define("VCT_ENABLED", m_vct_enabled)
    );
}

void ProbeManager::SetSphericalHarmonicsEnabled(bool value)
{
    if (m_spherical_harmonics_enabled == value) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("SPHERICAL_HARMONICS_ENABLED", value)
    );

    m_spherical_harmonics_enabled = value;
}

void ProbeManager::SetEnvMapEnabled(bool value)
{
    if (value == m_env_map_enabled) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("PROBE_ENABLED", value)
    );

    m_env_map_enabled = value;
}

void ProbeManager::SetVCTEnabled(bool vct_enabled)
{
    if (vct_enabled == m_vct_enabled) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("VCT_ENABLED", vct_enabled)
    );

    m_vct_enabled = vct_enabled;
}
} // namespace apex