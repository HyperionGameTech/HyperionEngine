/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <GraphicsPipeline.generated.inl>

namespace Hyperion {

GraphicsPipelineBase::~GraphicsPipelineBase()
{
    SafeDelete(std::move(m_shader));
}

RendererResult GraphicsPipelineBase::Create()
{
    if (!m_shader.IsValid())
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

    HYPERION_RETURN_OK;
}

uint32 GraphicsPipelineBase::GetDescriptorSetIndex(StringHash nameHash) const
{
    const DescriptorTableDeclaration* decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    if (decl == nullptr)
    {
        return ~0u;
    }

    return decl->GetDescriptorSetIndex(nameHash);
}

void GraphicsPipelineBase::SetShader(const ShaderRef& shader)
{
    m_shader = shader;
}

void GraphicsPipelineBase::SetRenderTargetDesc(const RenderTargetDesc& renderTargetDesc)
{
    m_renderTargetDesc = renderTargetDesc;
}

bool GraphicsPipelineBase::MatchesSignature(
    const Shader* shader,
    const RenderTargetDesc* renderTargetDesc,
    const RenderableAttributeSet& attributes) const
{
    // check shader:
    // if shader presence differs, no match
    if (!shader != !m_shader)
    {
        return false;
    }

    // if no render target desc provided we assume the caller doesn't care and don't bother checking
    if (renderTargetDesc && *renderTargetDesc != m_renderTargetDesc)
    {
        return false;
    }

    if (shader != nullptr && shader != m_shader)
    {
        if (shader->GetCompiledShader()->GetDefinition() != m_shader->GetCompiledShader()->GetDefinition())
        {
            return false;
        }
    }

    return true;
}

} // namespace Hyperion
