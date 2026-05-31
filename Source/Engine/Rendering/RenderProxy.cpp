#include <RenderingPch.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/AccelerationStructure.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <RenderProxy.generated.inl>

namespace Hyperion {

MeshRayTracingData::~MeshRayTracingData()
{
    if (blas != nullptr)
    {
        EnqueueDeletion(std::move(blas));
    }
}

} // namespace Hyperion
