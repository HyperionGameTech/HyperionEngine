/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>

namespace Hyperion {

class RenderableAttributeSet;
struct ShaderInputGroup;

struct PSOCacheKey
{
    HashCode hashCode;

    // keep shader name and properties around so we can expire PSOs when shaders are reloaded
    Name shaderName;
    ShaderPropertySet shaderProperties;

    PSOCacheKey()
    {
    }

    PSOCacheKey(
        const RenderableAttributeSet& attributes,
        const FramebufferDesc& framebufferDesc);

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

    HYP_FORCE_INLINE const VertexInputLayoutDesc& GetInputLayout() const
    {
        return m_inputLayout;
    }

    HYP_FORCE_INLINE void SetInputLayout(const VertexInputLayoutDesc& inputLayout)
    {
        m_inputLayout = inputLayout;
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

    HYP_FORCE_INLINE DepthCompareOp GetDepthCompareOp() const
    {
        return m_depthCompareOp;
    }

    HYP_FORCE_INLINE void SetDepthCompareOp(DepthCompareOp depthCompareOp)
    {
        m_depthCompareOp = depthCompareOp;
    }

    HYP_FORCE_INLINE bool GetDepthClamp() const
    {
        return m_depthClamp;
    }

    HYP_FORCE_INLINE void SetDepthClamp(bool depthClamp)
    {
        m_depthClamp = depthClamp;
    }

    HYP_FORCE_INLINE int GetDepthBias() const
    {
        return m_depthBias;
    }

    HYP_FORCE_INLINE void SetDepthBias(int depthBias)
    {
        m_depthBias = depthBias;
    }

    HYP_FORCE_INLINE float GetDepthBiasSlope() const
    {
        return m_depthBiasSlope;
    }

    HYP_FORCE_INLINE void SetDepthBiasSlope(float depthBiasSlope)
    {
        m_depthBiasSlope = depthBiasSlope;
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

    HYP_FORCE_INLINE const FramebufferDesc& GetFramebufferDesc() const
    {
        return m_framebufferDesc;
    }

    void SetFramebufferDesc(const FramebufferDesc& framebufferDesc);

#if HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE const PSOCacheKey& GetKey() const
    {
        return m_key;
    }

    HYP_FORCE_INLINE void SetKey(const PSOCacheKey& key)
    {
        m_key = key;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create();

    uint32 GetDescriptorSetIndex(StringHash nameHash) const;

    virtual void Bind(CommandBuffer* commandBuffer) = 0;
    virtual void Bind(CommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent) = 0;

    bool MatchesSignature(
        const RenderableAttributeSet& attributes,
        const FramebufferDesc& framebufferDesc) const;

    virtual void UpdateDynamicStates(CommandBuffer* cmd)
    {
    }

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

    VertexInputLayoutDesc m_inputLayout = {};

    Topology m_topology = TOP_TRIANGLES;
    FaceCullMode m_faceCullMode = FCM_BACK;
    FillMode m_fillMode = FM_FILL;
    BlendFunction m_blendFunction = BlendFunction::None();

    bool m_depthTest : 1 = true;
    bool m_depthWrite : 1 = true;
    bool m_depthClamp : 1 = false;

    bool m_stencilWrite : 1 = false;

    DepthCompareOp m_depthCompareOp = DCO_LESS;

    Optional<StencilFunction> m_stencilFunction;

    int m_depthBias = 0;
    float m_depthBiasSlope = 0.0f;

    ShaderInstanceRef m_shaderInstance;
    FramebufferDesc m_framebufferDesc;

    PSOCacheKey m_key;

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanGraphicsPipeline.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
