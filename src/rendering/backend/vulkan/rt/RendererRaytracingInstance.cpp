#include "RendererRaytracingInstance.hpp"

namespace hyperion {
namespace renderer {

RaytracingInstance::RaytracingInstance()
{
    
}

RaytracingInstance::~RaytracingInstance() = default;

Result RaytracingInstance::Create(Device *device, DescriptorPool *pool)
{
    auto result = Result::OK;

    for (size_t i = 0; i < m_raytracing_pipelines.size(); i++) {
        HYPERION_PASS_ERRORS(
            m_raytracing_pipelines[i]->Create(device, pool),
            result
        );

        if (!result) {
            for (auto j = static_cast<int64_t>(i - 1); j >= 0; j--) {
                HYPERION_IGNORE_ERRORS(m_raytracing_pipelines[j]->Destroy(device));
            }

            break;
        }
    }

    return result;
}

Result RaytracingInstance::Destroy(Device *device)
{
    auto result = Result::OK;

    for (auto &pipeline : m_raytracing_pipelines) {
        HYPERION_PASS_ERRORS(
            pipeline->Destroy(device),
            result
        );
    }

    return result;
}

} // namespace renderer
} // namespace hyperion
