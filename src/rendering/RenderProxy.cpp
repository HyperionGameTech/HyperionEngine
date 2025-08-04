#include <rendering/RenderProxy.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <scene/Entity.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern HYP_API SafeDeleter* g_safeDeleter;

#pragma region MeshRaytracingData

MeshRaytracingData::~MeshRaytracingData()
{
    SafeRelease(std::move(bottomLevelAccelerationStructures));
}

#pragma endregion MeshRaytracingData

} // namespace hyperion