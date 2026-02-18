/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <GraphicsPipeline.generated.inl>

namespace Hyperion {

#pragma region PSOCacheKey

PSOCacheKey::PSOCacheKey(
    const RenderableAttributeSet& attributes,
    const RenderTargetDesc& renderTargetDesc)
{
    hashCode = HashCode::GetHashCode(attributes.GetHashCode())
        .Combine(renderTargetDesc.GetHashCode());
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

    if (m_renderTargetDesc.numAttachments == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no attachment descriptors!");
    }

    RendererResult rebuildResult = Rebuild();

    if (!rebuildResult)
    {
        return rebuildResult;
    }

    return {};
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

void GraphicsPipelineBase::SetRenderTargetDesc(const RenderTargetDesc& renderTargetDesc)
{
    m_renderTargetDesc = renderTargetDesc;
}

bool GraphicsPipelineBase::MatchesSignature(
    const RenderableAttributeSet& attributes,
    const RenderTargetDesc& renderTargetDesc) const
{

    if (renderTargetDesc != m_renderTargetDesc)
        return false;

    const MeshAttributes& meshAttributes = attributes.GetMeshAttributes();

    if (meshAttributes.topology != m_topology || meshAttributes.vertexAttributes != m_vertexAttributes)
        return false;

    const MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();

    if (materialAttributes.blendFunction != m_blendFunction
        || materialAttributes.cullFaces != m_faceCullMode
        || materialAttributes.fillMode != m_fillMode
        || bool(materialAttributes.flags & MAF_DEPTH_TEST) != m_depthTest
        || bool(materialAttributes.flags & MAF_DEPTH_WRITE) != m_depthWrite)
    {
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

    if (materialAttributes.shaderName != shader.name
        || (shader.vertexAttributes.flagMask & meshAttributes.vertexAttributes.flagMask) != shader.vertexAttributes.flagMask
        || materialAttributes.shaderProperties != shader.properties)
    {
        return false;
    }

    return true;
}

#pragma endregion GraphicsPipelineBase

} // namespace Hyperion
