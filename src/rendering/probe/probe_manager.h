#ifndef PROBE_MANAGER_H
#define PROBE_MANAGER_H

#include "probe.h"
#include "gi/gi_mapper.h"
#include "envmap/envmap_probe.h"
#include "../camera/camera.h"

#include <array>
#include <memory>

namespace hyperion {

class Shader;
class ComputeShader;
class Renderer;

class ProbeManager {
public:
    ProbeManager();
    ~ProbeManager() = default;

    static const int voxel_map_size;
    static const float voxel_map_scale;
    static const int voxel_map_num_mipmaps;

    inline void AddProbe(const std::shared_ptr<Probe> &probe) { m_probes.push_back(probe); }
    inline void RemoveProbe(const std::shared_ptr<Probe> &probe)
    {
        auto it = std::find(m_probes.begin(), m_probes.end(), probe);

        if (it == m_probes.end()) {
            return;
        }

        m_probes.erase(it);
    }

    inline std::shared_ptr<Probe> &GetProbe(int index) { return m_probes[index]; }
    inline const std::shared_ptr<Probe> &GetProbe(int index) const { return m_probes[index]; }
    inline size_t NumProbes() const { return m_probes.size(); }

    inline bool SphericalHarmonicsEnabled() const { return m_spherical_harmonics_enabled; }
    void SetSphericalHarmonicsEnabled(bool value);
    inline bool VCTEnabled() const { return m_vct_enabled; }
    void SetVCTEnabled(bool value);
    inline bool EnvMapEnabled() const { return m_env_map_enabled; }
    void SetEnvMapEnabled(bool value);


    static ProbeManager *GetInstance();

private:
    std::vector<std::shared_ptr<Probe>> m_probes;
    bool m_spherical_harmonics_enabled;
    bool m_env_map_enabled;
    bool m_vct_enabled;

    static ProbeManager *instance;
};

} // namespace apex

#endif