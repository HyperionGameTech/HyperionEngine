#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/raytracing/RenderAccelerationStructure.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <RenderProxy.generated.inl>

namespace Hyperion {

MeshRaytracingData::~MeshRaytracingData()
{
    if (blas != nullptr)
    {
        SafeDelete(std::move(blas));
    }
}

} // namespace Hyperion
