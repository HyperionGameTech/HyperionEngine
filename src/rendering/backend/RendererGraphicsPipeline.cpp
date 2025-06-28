/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {

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

    RendererResult rebuild_result = Rebuild();

    if (!rebuild_result)
    {
        return rebuild_result;
    }

    HYPERION_RETURN_OK;
}

RendererResult GraphicsPipelineBase::Destroy()
{
    SafeRelease(std::move(m_framebuffers));
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));

    HYPERION_RETURN_OK;
}

void GraphicsPipelineBase::SetDescriptorTable(const DescriptorTableRef& descriptor_table)
{
    m_descriptor_table = descriptor_table;
}

void GraphicsPipelineBase::SetShader(const ShaderRef& shader)
{
    m_shader = shader;
}

void GraphicsPipelineBase::SetFramebuffers(const Array<FramebufferRef>& framebuffers)
{
    SafeRelease(std::move(m_framebuffers));
    m_framebuffers = framebuffers;
}

bool GraphicsPipelineBase::MatchesSignature(
    const ShaderBase* shader,
    const DescriptorTableDeclaration& descriptor_table_decl,
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

    if (descriptor_table_decl.GetHashCode() != m_descriptor_table->GetDeclaration()->GetHashCode())
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