#ifndef GI_MANAGER_H
#define GI_MANAGER_H

#include "gi_mapper.h"
#include "../camera/camera.h"

#include <array>
#include <memory>

namespace hyperion {

class Shader;
class ComputeShader;
class Renderer;

class GIManager {
public:
    GIManager();
    ~GIManager() = default;

    static const int voxel_map_size;
    static const float voxel_map_scale;

    inline void AddProbe(const std::shared_ptr<GIMapper> &mapper) { m_gi_map_renderers.push_back(mapper); }
    inline void RemoveProbe(const std::shared_ptr<GIMapper> &mapper)
    {
        auto it = std::find(m_gi_map_renderers.begin(), m_gi_map_renderers.end(), mapper);

        if (it == m_gi_map_renderers.end()) {
            return;
        }

        m_gi_map_renderers.erase(it);
    }

    inline std::shared_ptr<GIMapper> &GetProbe(int index) { return m_gi_map_renderers[index]; }
    inline const std::shared_ptr<GIMapper> &GetProbe(int index) const { return m_gi_map_renderers[index]; }
    inline size_t NumProbes() const { return m_gi_map_renderers.size(); }

    static GIManager *GetInstance();

private:
    std::vector<std::shared_ptr<GIMapper>> m_gi_map_renderers;

    static GIManager *instance;
};

} // namespace apex

#endif