/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Attachment.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Attachment

DX12Attachment::DX12Attachment(
    const DX12GpuImageRef& image,
    const DX12FramebufferWeakRef& framebuffer,
    RenderTargetType renderTargetType,
    LoadOperation loadOperation,
    StoreOperation storeOperation,
    BlendFunction blendFunction)
    : AttachmentBase(image, framebuffer, loadOperation, storeOperation, blendFunction),
      m_renderTargetType(renderTargetType)
{
    m_imageView = CreateObject<DX12GpuImageView>(image);
}

DX12Attachment::~DX12Attachment()
{
    SafeDelete(std::move(m_image));
    SafeDelete(std::move(m_imageView));
}

bool DX12Attachment::IsCreated() const
{
    return false;
}

RendererResult DX12Attachment::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

#pragma endregion DX12Attachment

} // namespace Hyperion
