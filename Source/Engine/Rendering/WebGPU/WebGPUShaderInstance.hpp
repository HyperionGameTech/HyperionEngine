/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/ShaderInstance.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>

namespace Hyperion {

enum class WebGPUBindingKind : uint8
{
    UniformBuffer,
    ReadOnlyStorageBuffer,
    StorageBuffer,
    Sampler,
    ComparisonSampler,
    Texture,
    DepthTexture,
    StorageTexture
};

struct WebGPUReflectedBinding
{
    uint32 group = 0;
    uint32 binding = 0;
    WebGPUBindingKind kind = WebGPUBindingKind::UniformBuffer;
    WGPUTextureViewDimension viewDimension = WGPUTextureViewDimension_Undefined;
    WGPUTextureSampleType sampleType = WGPUTextureSampleType_Float;
    WGPUTextureFormat storageFormat = WGPUTextureFormat_Undefined;
    WGPUStorageTextureAccess storageAccess = WGPUStorageTextureAccess_WriteOnly;
    bool isStorageTextureRead = false;
    WGPUShaderStage visibility = WGPUShaderStage_None;
};

struct WebGPUReflectedGroup
{
    Array<WebGPUReflectedBinding, WebGPUAllocator> bindings;
    uint64 hashCode = 0;
};

// The bindings a shader declares are read back out of its WGSL, since bind group layouts have to match them exactly
HYP_CLASS(NoScriptBindings)
class WebGPUShaderInstance final : public ShaderInstanceBase
{
    HYP_OBJECT_BODY(WebGPUShaderInstance);

    struct ShaderModuleVariant
    {
        uint64 key;
        WGPUShaderModule shaderModule;
    };

    struct ShaderModule
    {
        ShaderModuleType type;
        WGPUShaderModule shaderModule;
        ANSIString entryPointName;

        ANSIString source;
        Array<ShaderModuleVariant, WebGPUAllocator> variants;
    };

public:
    WebGPUShaderInstance();
    explicit WebGPUShaderInstance(const Shader* shader);
    ~WebGPUShaderInstance() override;

    WGPUShaderModule GetShaderModule(ShaderModuleType type) const;

    // DXC writes rgba32float for every untyped RWTexture, and WebGPU needs the declared format to equal the bound one,
    // so the module is rebuilt with the declarations rewritten for the formats it is about to be used with.
    WGPUShaderModule GetShaderModuleVariant(ShaderModuleType type, Span<const WebGPUStorageTextureOverride> overrides);
    ANSIStringView GetEntryPointName(ShaderModuleType type) const;

    HYP_FORCE_INLINE uint32 GetNumBindGroups() const
    {
        return m_numBindGroups;
    }

    HYP_FORCE_INLINE const WebGPUReflectedGroup& GetReflectedGroup(uint32 groupIndex) const
    {
        return m_reflectedGroups[groupIndex];
    }

    HYP_FORCE_INLINE uint32 GetFragmentOutputMask() const
    {
        return m_fragmentOutputMask;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    bool Reflect(ShaderModuleType moduleType, const char* source, size_t length);
    void ReflectFragmentOutputs(ANSIStringView entryPointName, const char* source, size_t length);

    Array<ShaderModule, WebGPUAllocator> m_shaderModules;

    WebGPUReflectedGroup m_reflectedGroups[WebGPUCommandBuffer::MaxBindGroups];
    uint32 m_numBindGroups;
    uint32 m_fragmentOutputMask;
};

} // namespace Hyperion
