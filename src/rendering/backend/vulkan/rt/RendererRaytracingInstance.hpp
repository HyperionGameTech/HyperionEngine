#ifndef HYPERION_RENDERER_RAYTRACING_INSTANCE_H
#define HYPERION_RENDERER_RAYTRACING_INSTANCE_H

#include "RendererRaytracingPipeline.hpp"

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {

class RaytracingInstance {
public:
    RaytracingInstance();
    RaytracingInstance(const RaytracingInstance &other) = delete;
    RaytracingInstance &operator=(const RaytracingInstance &other) = delete;
    ~RaytracingInstance();

    inline void AddRaytracingPipeline(std::unique_ptr<RaytracingPipeline> &&raytracing_pipeline)
        { m_raytracing_pipelines.push_back(std::move(raytracing_pipeline)); }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

private:
    std::vector<std::unique_ptr<RaytracingPipeline>> m_raytracing_pipelines;
};

} // namespace renderer
} // namespace hyperion

#endif