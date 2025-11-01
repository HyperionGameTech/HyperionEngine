/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderShader.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {

GraphicsPipelineBase::~GraphicsPipelineBase()
{
    SafeDelete(std::move(m_descriptorTable));
    SafeDelete(std::move(m_shader));
    SafeDelete(std::move(m_framebuffers));
}

RendererResult GraphicsPipelineBase::Create()
{
    if (!m_shader.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no shader");
    }

    if (m_framebuffers.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no framebuffers");
    }

    RendererResult rebuildResult = Rebuild();

    if (!rebuildResult)
    {
        return rebuildResult;
    }

    HYPERION_RETURN_OK;
}

void GraphicsPipelineBase::SetDescriptorTable(const DescriptorTableRef& descriptorTable)
{
    m_descriptorTable = descriptorTable;
}

void GraphicsPipelineBase::SetShader(const ShaderRef& shader)
{
    m_shader = shader;
}

void GraphicsPipelineBase::SetFramebuffers(const Array<FramebufferRef>& framebuffers)
{
    SafeDelete(std::move(m_framebuffers));
    m_framebuffers = framebuffers;
}

bool GraphicsPipelineBase::MatchesSignature(
    const ShaderBase* shader,
    const DescriptorTableDeclaration& descriptorTableDecl,
    const Array<const FramebufferBase*>& framebuffers,
    const RenderableAttributeSet& attributes) const
{
    if (!shader != !m_shader)
    {
        return false;
    }

    if (m_framebuffers.Size() != framebuffers.Size())
    {
        return false;
    }

    if (shader != nullptr)
    {
        if (shader->GetCompiledShader()->GetHashCode() != m_shader->GetCompiledShader()->GetHashCode())
        {
            return false;
        }
    }

    if (descriptorTableDecl.GetHashCode() != m_descriptorTable->GetDeclaration()->GetHashCode())
    {
        return false;
    }

    for (SizeType i = 0; i < m_framebuffers.Size(); i++)
    {
        if (m_framebuffers[i].Get() != framebuffers[i])
        {
            return false;
        }
    }

    return true;
}

} // namespace hyperion
