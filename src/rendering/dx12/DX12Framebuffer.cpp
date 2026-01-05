/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Framebuffer.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Framebuffer

DX12Framebuffer::DX12Framebuffer(Vec2u extent, RenderTargetType renderTargetType, uint32 numViews)
    : FramebufferBase(extent, renderTargetType)
{
}

DX12Framebuffer::~DX12Framebuffer()
{
}

bool DX12Framebuffer::IsCreated() const
{
    return false;
}

RendererResult DX12Framebuffer::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12Framebuffer::Resize(Vec2u newSize)
{
    m_extent = newSize;
    // @TODO
    HYPERION_RETURN_OK;
}

AttachmentRef DX12Framebuffer::AddAttachment(const AttachmentRef& attachment)
{
    // @TODO
    return attachment;
}

AttachmentRef DX12Framebuffer::AddAttachment(uint32 binding, const GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp)
{
    // @TODO
    return AttachmentRef();
}

AttachmentRef DX12Framebuffer::AddAttachment(
    uint32 binding,
    TextureFormat format,
    TextureType type,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    // @TODO
    return AttachmentRef();
}

bool DX12Framebuffer::RemoveAttachment(uint32 binding)
{
    // @TODO
    return false;
}

AttachmentBase* DX12Framebuffer::GetAttachment(uint32 binding) const
{
    // @TODO
    return nullptr;
}

int DX12Framebuffer::NumAttachments() const
{
    // @TODO
    return 0;
}

void DX12Framebuffer::BeginCapture(CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12Framebuffer::EndCapture(CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12Framebuffer::Clear(CommandBuffer* commandBuffer)
{
    // @TODO
}

#pragma endregion DX12Framebuffer

} // namespace Hyperion
