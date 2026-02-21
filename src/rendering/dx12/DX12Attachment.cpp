/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <DX12Attachment.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Attachment

DX12Attachment::DX12Attachment(
    const DX12GpuImageRef& image,
    const DX12FramebufferWeakRef& framebuffer,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, framebuffer, attachmentDesc)
{
    m_imageView = MakeHandle<DX12GpuImageView>(image);
}

DX12Attachment::~DX12Attachment()
{
    EnqueueDeletion(std::move(m_image));
    EnqueueDeletion(std::move(m_imageView));
}

bool DX12Attachment::IsCreated() const
{
    return m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult DX12Attachment::Create()
{
    Assert(m_image != nullptr && m_imageView != nullptr);

    if (!m_image->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_imageView->Create();
}

#pragma endregion DX12Attachment

} // namespace Hyperion
