/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Attachment.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

struct AttachmentDesc;

HYP_CLASS(NoScriptBindings)
class DX12Attachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(DX12Attachment);

public:
    DX12Attachment(
        const DX12GpuImageRef& image,
        const DX12FramebufferWeakRef& framebuffer,
        const AttachmentDesc& attachmentDesc);
    ~DX12Attachment() override;

    bool IsCreated() const override;

    RendererResult Create() override;
};

} // namespace Hyperion
