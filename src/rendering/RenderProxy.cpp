#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/AccelerationStructure.hpp>

#include <rendering/util/DeletionQueue.hpp>

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
