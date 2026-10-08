/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Shared.hpp>
#include <Rendering/RenderMemory.hpp>

#include <webgpu/webgpu.h>

#include <Rendering/WebGPU/WebGPUThreadProxy.hpp>

namespace Hyperion {

HYP_FORCE_INLINE WGPUStringView ToWGPUStringView(const char* str)
{
    return WGPUStringView { str, WGPU_STRLEN };
}

HYP_FORCE_INLINE WGPUStringView ToWGPUStringView(const char* str, size_t length)
{
    return WGPUStringView { str, length };
}

struct WebGPUStorageTextureOverride
{
    uint32 group;
    uint32 binding;
    WGPUTextureFormat format;
    WGPUStorageTextureAccess access;
};

HYP_FORCE_INLINE ANSIStringView ToStringView(WGPUStringView stringView)
{
    if (stringView.data == nullptr)
    {
        return ANSIStringView();
    }

    const size_t length = stringView.length == WGPU_STRLEN ? std::strlen(stringView.data) : stringView.length;

    return ANSIStringView(stringView.data, stringView.data + length);
}

} // namespace Hyperion
