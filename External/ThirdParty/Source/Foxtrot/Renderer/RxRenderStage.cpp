#include "RxRenderStage.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


RxTarget* RxRenderStage::GetTarget(RxImageFormat format, int sub_index)
{
    for (RxTarget& attachment : mOutputTargets.Targets) {
        if (attachment.Image.Format == format) {
            if ((sub_index--) > 0) {
                continue;
            }

            return &attachment;
        }
    }

    return nullptr;
}


void RxRenderStage::BuildRenderStage()
{
    if (mbIsBuilt) {
        return;
    }

    if (mbIsFinalStage) {
        AddPresentTarget();
    }

    // If there are no clear values already defined, generate them
    if (ClearValues.Size == 0) {
        MakeClearValues();
    }

    mOutputTargets.CreateImages();

    mRenderPass.Create(mOutputTargets, mSize);

    if (mbIsFinalStage) {
        CreateFinalStageFramebuffers();
    }
    else {
        mFramebuffer.Create(mOutputTargets.GetImageViews(), mRenderPass, mSize);
    }

    mbIsBuilt = true;
}

void RxRenderStage::Begin(RxCommandBuffer& cmd, RxPipeline& pipeline)
{
    FxAssert(mbIsBuilt);

    VkFramebuffer framebuffer;

    if (mbIsFinalStage) {
        framebuffer = mFinalStageFramebuffers[gRenderer->GetImageIndex()].Get();
    }
    else {
        framebuffer = mFramebuffer.Get();
    }

    mRenderPass.Begin(&cmd, framebuffer, ClearValues);
    pipeline.Bind(cmd);
}

void RxRenderStage::AddTarget(RxImageFormat format, const FxVec2u& size, VkImageUsageFlags usage,
                              RxImageAspectFlag aspect)
{
    mOutputTargets.Add(RxTarget(format, size, usage, aspect));
}

void RxRenderStage::AddTarget(const RxTarget& attachment) { mOutputTargets.Add(attachment); }


void RxRenderStage::MakeClearValues()
{
    for (const RxTarget& attachment : mOutputTargets.Targets) {
        if (attachment.bRenderPassOnly) {
            continue;
        }

        if (attachment.Aspect == RxImageAspectFlag::eDepth) {
            ClearValues.Insert(VkClearValue { .depthStencil = { 0.0f, 0U } });
        }
        else if (attachment.Aspect == RxImageAspectFlag::eColor) {
            ClearValues.Insert(VkClearValue { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } });
        }
    }
}


void RxRenderStage::CreateFinalStageFramebuffers()
{
    FxSizedArray<RxImage>& final_images = gRenderer->Swapchain.OutputImages;

    mFinalStageFramebuffers.InitSize(final_images.Size);

    FxSizedArray<VkImageView> image_views;
    image_views.InitSize(1);

    for (uint32 i = 0; i < final_images.Size; i++) {
        image_views[0] = final_images[i].View;
        mFinalStageFramebuffers[i].Create(image_views, mRenderPass, gRenderer->Swapchain.Extent);
    }
}


void RxRenderStage::MarkFinalStage()
{
    FxAssertMsg(mbIsBuilt == false, "Cannot mark final -- Render stage was already built!");

    mbIsFinalStage = true;
}


void RxRenderStage::AddPresentTarget()
{
    mOutputTargets.Add(RxTarget(gRenderer->Swapchain.Surface.Format, RxTarget::scFullScreen, RxLoadOp::eDontCare,
                                RxStoreOp::eStore, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
}
