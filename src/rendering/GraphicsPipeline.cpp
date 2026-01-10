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

    if (m_attachmentDescs.Empty())
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

void GraphicsPipelineBase::SetAttachmentDescs(const Array<AttachmentDesc>& attachmentDescs)
{
    m_attachmentDescs = attachmentDescs;
}

bool GraphicsPipelineBase::MatchesSignature(
    const Shader* shader,
    Span<const AttachmentDesc> attachmentDescs,
    const RenderableAttributeSet& attributes) const
{
    // check shader:
    // if shader presence differs, no match
    if (!shader != !m_shader)
    {
        return false;
    }

    // if any attachment descs are provided, check that the sizes match
    // (if no attachment descs are provided, we skip this check, assuming the caller doesn't care about the attachments matching)
    if (attachmentDescs.Size() != 0 && m_attachmentDescs.Size() != attachmentDescs.Size())
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

    if (attachmentDescs.Size() != 0)
    {
        for (SizeType i = 0; i < attachmentDescs.Size(); i++)
        {
            if (m_attachmentDescs[i] != attachmentDescs[i])
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace Hyperion
