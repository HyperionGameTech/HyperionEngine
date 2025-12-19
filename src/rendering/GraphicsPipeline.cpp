/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <GraphicsPipeline.generated.inl>

namespace hyperion {

GraphicsPipelineBase::~GraphicsPipelineBase()
{
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

void GraphicsPipelineBase::SetFramebuffers(const Array<FramebufferRef>& framebuffers)
{
    SafeDelete(std::move(m_framebuffers));
    m_framebuffers = framebuffers;
}

bool GraphicsPipelineBase::MatchesSignature(
    const Shader* shader,
    const Array<const Framebuffer*>& framebuffers,
    const RenderableAttributeSet& attributes) const
{
    // check shader:
    // if shader presence differs, no match
    if (!shader != !m_shader)
    {
        return false;
    }

    // if ANY framebuffer is provided, check that the sizes match
    // (if no framebuffers are provided, we skip this check, assuming the caller doesn't care about framebuffer matching)
    if (framebuffers.Size() != 0 && m_framebuffers.Size() != framebuffers.Size())
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

    if (framebuffers.Size() != 0)
    {
        for (SizeType i = 0; i < m_framebuffers.Size(); i++)
        {
            if (m_framebuffers[i].Get() != framebuffers[i])
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace hyperion
