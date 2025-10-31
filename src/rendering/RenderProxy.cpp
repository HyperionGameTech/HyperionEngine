#include <rendering/RenderProxy.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <RenderProxy.generated.inl>

namespace hyperion {

MeshRaytracingData::~MeshRaytracingData()
{
    if (blas != nullptr)
    {
        SafeDelete(std::move(blas));
    }
}

} // namespace hyperion
