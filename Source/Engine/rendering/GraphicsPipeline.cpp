/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <GraphicsPipeline.generated.inl>

namespace Hyperion {

#pragma region PSOCacheKey

PSOCacheKey::PSOCacheKey(
    const RenderableAttributeSet& attributes,
    const FramebufferDesc& framebufferDesc)
{
    hashCode = HashCode::GetHashCode(attributes.GetHashCode())
        .Combine(framebufferDesc.GetHashCode());

    shaderName = attributes.GetMaterialAttributes().shaderName;
    shaderProperties = attributes.GetMaterialAttributes().shaderProperties;
}

#pragma endregion PSOCacheKey

#pragma region GraphicsPipelineBase

GraphicsPipelineBase::~GraphicsPipelineBase()
{
}

RendererResult GraphicsPipelineBase::Create()
{
    if (!m_shaderInstance.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no shader");
    }

    if (m_framebufferDesc.numAttachments == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no attachment descriptors!");
    }

    return Rebuild();
}

uint32 GraphicsPipelineBase::GetDescriptorSetIndex(StringHash nameHash) const
{
    const ShaderInputGroup* decl = m_shaderInstance->GetShader()->GetDescriptorTableDeclaration();

    if (decl == nullptr)
    {
        return ~0u;
    }

    return decl->GetDescriptorSetIndex(nameHash);
}

void GraphicsPipelineBase::SetShader(const ShaderInstanceRef& shaderInstance)
{
    m_shaderInstance = shaderInstance;
}

void GraphicsPipelineBase::SetFramebufferDesc(const FramebufferDesc& framebufferDesc)
{
    m_framebufferDesc = framebufferDesc;
}

bool GraphicsPipelineBase::MatchesSignature(
    const RenderableAttributeSet& attributes,
    const FramebufferDesc& framebufferDesc) const
{
    //if (!m_framebufferDesc.IsPSOCompatible(framebufferDesc))
    if (m_framebufferDesc != framebufferDesc)
        return false;

    const MeshAttributes& meshAttributes = attributes.GetMeshAttributes();

    if (meshAttributes.topology != m_topology || meshAttributes.inputLayout != m_inputLayout)
        return false;

    const MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();

    if (materialAttributes.blendFunction != m_blendFunction
        || materialAttributes.cullFaces != m_faceCullMode
        || materialAttributes.fillMode != m_fillMode
        || bool(materialAttributes.flags & MAF_DEPTH_TEST) != m_depthTest
        || bool(materialAttributes.flags & MAF_DEPTH_WRITE) != m_depthWrite
        || bool(materialAttributes.flags & MAF_DEPTH_CLAMP) != m_depthClamp)
    {
        return false;
    }

    if (materialAttributes.flags & MAF_DEPTH_BIAS)
    {
        if (m_depthBias != materialAttributes.depthBias || !MathUtil::ApproxEqual(m_depthBiasSlope, materialAttributes.depthBiasSlope))
            return false;
    }
    else
    {
        if (m_depthBias != 0)
            return false;
    }

    if (materialAttributes.flags & MAF_STENCIL_TEST)
    {
        if (!m_stencilFunction.HasValue())
            return false;

        if (*m_stencilFunction != materialAttributes.stencilFunction)
            return false;
    }
    else
    {
        if (m_stencilFunction.HasValue())
            return false;
    }

    const Shader& shader = *m_shaderInstance->GetShader();

    if (materialAttributes.shaderName != shader.baseName
        || ((shader.properties & materialAttributes.shaderProperties) != shader.properties)
        || (shader.inputLayout.mask & meshAttributes.inputLayout.mask) != shader.inputLayout.mask)
    {
        return false;
    }

    return true;
}

#pragma endregion GraphicsPipelineBase

} // namespace Hyperion
