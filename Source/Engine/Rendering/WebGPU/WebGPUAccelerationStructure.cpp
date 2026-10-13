/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUAccelerationStructure.hpp>

#include <WebGPUAccelerationStructure.generated.inl>

namespace Hyperion {

#pragma region WebGPUTopLevelAS

WebGPUTopLevelAS::WebGPUTopLevelAS(const ASResourceCallbacks& callbacks)
    : TopLevelASBase(callbacks)
{
}

WebGPUTopLevelAS::~WebGPUTopLevelAS() = default;

bool WebGPUTopLevelAS::IsCreated() const
{
    return false;
}

void WebGPUTopLevelAS::AddBLAS(uint64 key, WebGPUBottomLevelAS* blas)
{
}

bool WebGPUTopLevelAS::RemoveBLAS(uint64 key)
{
    return false;
}

bool WebGPUTopLevelAS::ContainsBLAS(uint64 key)
{
    return false;
}

RendererResult WebGPUTopLevelAS::Create()
{
    return HYP_MAKE_ERROR(RendererError, "Ray tracing is not supported on WebGPU");
}

RendererResult WebGPUTopLevelAS::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    return {};
}

#pragma endregion WebGPUTopLevelAS

#pragma region WebGPUBottomLevelAS

WebGPUBottomLevelAS::WebGPUBottomLevelAS()
    : BottomLevelASBase()
{
}

WebGPUBottomLevelAS::~WebGPUBottomLevelAS() = default;

bool WebGPUBottomLevelAS::IsCreated() const
{
    return false;
}

RendererResult WebGPUBottomLevelAS::Create()
{
    return HYP_MAKE_ERROR(RendererError, "Ray tracing is not supported on WebGPU");
}

void WebGPUBottomLevelAS::SetTransform(const Mat4f& transform)
{
}

#pragma endregion WebGPUBottomLevelAS

} // namespace Hyperion
