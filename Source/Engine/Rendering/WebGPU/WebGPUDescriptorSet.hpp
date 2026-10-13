/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>

namespace Hyperion {

struct WebGPUReflectedGroup;

// A bind group layout has to match the shader it is used with binding for binding, so a set builds one bind group
// per distinct shader interface it gets bound against, and drops them all when one of its elements changes.
HYP_CLASS(NoScriptBindings)
class WebGPUDescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(WebGPUDescriptorSet);

public:
    explicit WebGPUDescriptorSet(const DescriptorSetLayout& layout);
    ~WebGPUDescriptorSet() override;

    bool IsCreated() const override;
    RendererResult Create() override;
    void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    void Update(bool force = false) override;

    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUGraphicsPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUComputePipeline* pipeline, uint32 bindIndex) const override;
    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPURayTracingPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(WebGPUCommandBuffer* commandBuffer, const WebGPURayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    WebGPUDescriptorSetRef Clone() const override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    struct CachedBindGroup
    {
        uint64 reflectionHashCode = 0;
        WGPUBindGroupLayout layout = nullptr;
        WGPUBindGroup bindGroup = nullptr;

        uint32 numDynamicElements = 0;
        Name dynamicElementNames[WebGPUBoundBindGroup::MaxDynamicOffsets];
        uint32 dynamicElementAlignments[WebGPUBoundBindGroup::MaxDynamicOffsets] = {};
        bool dynamicElementRealigned[WebGPUBoundBindGroup::MaxDynamicOffsets] = {};
    };

    void BindInternal(WebGPUCommandBuffer* commandBuffer, bool isCompute, const WebGPUReflectedGroup& reflectedGroup, const DescriptorSetOffsetMap* offsets, uint32 bindIndex) const;

    const CachedBindGroup* GetOrCreateBindGroup(const WebGPUReflectedGroup& reflectedGroup) const;
    const ShaderInputWithBinding* FindElementByBinding(uint32 binding) const;

    uint64 CalculateStateHashCode() const;
    void ReleaseBindGroups();

    mutable Array<CachedBindGroup, WebGPUAllocator> m_bindGroups;

    uint64 m_stateHashCode;
    uint64 m_pendingStateHashCode;
    bool m_isCreated;
};

HYP_CLASS(NoScriptBindings)
class WebGPUDescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(WebGPUDescriptorTable);

public:
    explicit WebGPUDescriptorTable(const ShaderInputGroup* decl);
    ~WebGPUDescriptorTable() override;
};

} // namespace Hyperion
