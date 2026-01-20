#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/AccelerationStructure.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <RenderProxy.generated.inl>

namespace Hyperion {

MeshRayTracingData::~MeshRayTracingData()
{
    if (blas != nullptr)
    {
        SafeDelete(std::move(blas));
    }
}

} // namespace Hyperion
