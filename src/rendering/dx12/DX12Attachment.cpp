/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <DX12Attachment.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Attachment

DX12Attachment::DX12Attachment(
    const DX12GpuImageRef& image,
    const DX12FramebufferWeakRef& framebuffer,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, framebuffer, attachmentDesc)
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
    return {};
}

#pragma endregion DX12Attachment

} // namespace Hyperion
