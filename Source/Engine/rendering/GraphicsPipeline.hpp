/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/containers/Array.hpp>

namespace Hyperion {

class RenderableAttributeSet;
struct ShaderInputGroup;

struct PSOCacheKey
{
    HashCode hashCode;

    PSOCacheKey()
    {
    }

    PSOCacheKey(
        const RenderableAttributeSet& attributes,
        const RenderTargetDesc& renderTargetDesc);
    
    PSOCacheKey(const PSOCacheKey& other) = default;
    PSOCacheKey& operator=(const PSOCacheKey& other) = default;

    HYP_FORCE_INLINE constexpr bool operator==(const PSOCacheKey& other) const
    {
        return hashCode == other.hashCode;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const PSOCacheKey& other) const
    {
        return hashCode != other.hashCode;
    }

    HYP_FORCE_INLINE constexpr operator HashCode() const
    {
        return hashCode;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return hashCode;
    }
};

HYP_CLASS(Abstract, NoScriptBindings)
class GraphicsPipelineBase : public ObjectBase
{
    HYP_OBJECT_BODY(GraphicsPipelineBase);

public:
    virtual ~GraphicsPipelineBase() override;

    static Pool* GetAllocator() { return g_rhiPool; }

    HYP_FORCE_INLINE const VertexAttributeSet& GetVertexAttributes() const
    {
        return m_vertexAttributes;
    }

    HYP_FORCE_INLINE void SetVertexAttributes(const VertexAttributeSet& vertexAttributes)
    {
        m_vertexAttributes = vertexAttributes;
    }

    HYP_FORCE_INLINE Topology GetTopology() const
    {
        return m_topology;
    }

    HYP_FORCE_INLINE void SetTopology(Topology topology)
    {
        m_topology = topology;
    }

    HYP_FORCE_INLINE FaceCullMode GetCullMode() const
    {
        return m_faceCullMode;
    }

    HYP_FORCE_INLINE void SetCullMode(FaceCullMode faceCullMode)
    {
        m_faceCullMode = faceCullMode;
    }

    HYP_FORCE_INLINE FillMode GetFillMode() const
    {
        return m_fillMode;
    }

    HYP_FORCE_INLINE void SetFillMode(FillMode fillMode)
    {
        m_fillMode = fillMode;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blendFunction;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blendFunction)
    {
        m_blendFunction = blendFunction;
    }

    HYP_FORCE_INLINE bool GetDepthTest() const
    {
        return m_depthTest;
    }

    HYP_FORCE_INLINE void SetDepthTest(bool depthTest)
    {
        m_depthTest = depthTest;
    }

    HYP_FORCE_INLINE bool GetDepthWrite() const
    {
        return m_depthWrite;
    }

    HYP_FORCE_INLINE void SetDepthWrite(bool depthWrite)
    {
        m_depthWrite = depthWrite;
    }

    HYP_FORCE_INLINE const Optional<StencilFunction>& GetStencilFunction() const
    {
        return m_stencilFunction;
    }

    HYP_FORCE_INLINE void SetStencilFunction(const Optional<StencilFunction>& optStencilFunction)
    {
        m_stencilFunction = optStencilFunction;
    }

    HYP_FORCE_INLINE bool GetStencilWrite() const
    {
        return m_stencilWrite;
    }

    HYP_FORCE_INLINE void SetStencilWrite(bool stencilWrite)
    {
        m_stencilWrite = stencilWrite;
    }

    HYP_FORCE_INLINE const ShaderInstanceRef& GetShader() const
    {
        return m_shaderInstance;
    }

    void SetShader(const ShaderInstanceRef& shader);

    HYP_FORCE_INLINE const RenderTargetDesc& GetRenderTargetDesc() const
    {
        return m_renderTargetDesc;
    }

    void SetRenderTargetDesc(const RenderTargetDesc& renderTargetDesc);
    
#ifdef HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE const PSOCacheKey& GetPSOCacheKey() const
    {
        return m_psoCacheKey;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create();

    uint32 GetDescriptorSetIndex(StringHash nameHash) const;

    virtual void Bind(CommandBuffer* commandBuffer) = 0;
    virtual void Bind(CommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent) = 0;

    bool MatchesSignature(
        const RenderableAttributeSet& attributes,
        const RenderTargetDesc& renderTargetDesc) const;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED virtual void SetPushConstants(const void* data, SizeType size) = 0;

    uint32 lastFrame = uint32(-1);

protected:
    GraphicsPipelineBase()
    {
    }

    explicit GraphicsPipelineBase(const ShaderInstanceRef& shaderInstance)
        : m_shaderInstance(shaderInstance)
    {
    }

    virtual RendererResult Rebuild() = 0;

    VertexAttributeSet m_vertexAttributes;

    Topology m_topology = TOP_TRIANGLES;
    FaceCullMode m_faceCullMode = FCM_BACK;
    FillMode m_fillMode = FM_FILL;
    BlendFunction m_blendFunction = BlendFunction::None();

    bool m_depthTest = true;
    bool m_depthWrite = true;

    bool m_stencilWrite = false;
    Optional<StencilFunction> m_stencilFunction;

    ShaderInstanceRef m_shaderInstance;
    RenderTargetDesc m_renderTargetDesc;
    
    PSOCacheKey m_psoCacheKey;

#ifdef HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
