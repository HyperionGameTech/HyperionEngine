#include "RxAttachment.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

RxTarget::RxTarget(RxImageFormat format, const FxVec2u& size)
{
    Image.Format = format;
    Image.Size = size;

    if (size == RxTarget::scFullScreen) {
        Image.Size = gRenderer->Swapchain.Extent;
    }
}

RxTarget::RxTarget(RxImageFormat format, const FxVec2u& size, RxLoadOp load_op, RxStoreOp store_op,
                   VkImageLayout initial_layout, VkImageLayout final_layout)
    : LoadOp(load_op), StoreOp(store_op), InitialLayout(initial_layout), FinalLayout(final_layout)
{
    Image.Format = format;
    Image.Size = size;

    if (size == RxTarget::scFullScreen) {
        Image.Size = gRenderer->Swapchain.Extent;
    }
}

RxTarget::RxTarget(RxImageFormat format, const FxVec2u& size, VkImageUsageFlags usage, RxImageAspectFlag aspect)
    : Usage(usage), Aspect(aspect)
{
    Image.Format = format;
    Image.Size = size;

    if (size == RxTarget::scFullScreen) {
        Image.Size = gRenderer->Swapchain.Extent;
    }
}


void RxTarget::CreateImage()
{
    if (bReuseImage) {
        return;
    }

    Image.Create(ImageType, Image.Size, Image.Format, VK_IMAGE_TILING_OPTIMAL, Usage, Aspect);
}


VkAttachmentDescription RxTarget::BuildDescription() const
{
    FxAssert(Image.Format != RxImageFormat::eNone);

    return VkAttachmentDescription {
        .format = RxImageFormatUtil::ToUnderlying(Image.Format),
        .samples = Samples,

        .loadOp = static_cast<VkAttachmentLoadOp>(LoadOp),
        .storeOp = static_cast<VkAttachmentStoreOp>(StoreOp),

        .stencilLoadOp = static_cast<VkAttachmentLoadOp>(StencilLoadOp),
        .stencilStoreOp = static_cast<VkAttachmentStoreOp>(StencilStoreOp),

        .initialLayout = InitialLayout,
        .finalLayout = FinalLayout,
    };
}


///////////////////////////////////
// Attachment List Functions
///////////////////////////////////


FxSizedArray<VkAttachmentDescription>& RxTargetList::GetDescriptions()
{
    // Return the descriptions if they are already built
    if (mbDescriptionsBuilt) {
        return mBuiltAttachmentDescriptions;
    }

    if (!mBuiltAttachmentDescriptions) {
        mBuiltAttachmentDescriptions.InitCapacity(mMaxTargets);
    }

    mBuiltAttachmentDescriptions.Clear();

    for (const RxTarget& at : Targets) {
        mBuiltAttachmentDescriptions.Insert(at.BuildDescription());
    }

    mbDescriptionsBuilt = true;

    return mBuiltAttachmentDescriptions;
}


RxTargetList& RxTargetList::Add(const RxTarget& attachment)
{
    CheckInited();
    Targets.Insert(attachment);

    mbImageViewsBuilt = false;

    return *this;
}

RxTargetList& RxTargetList::Add(const RxTarget* attachment)
{
    FxAssertMsg(attachment != nullptr, "Attachment cannot be null!");
    return Add(*attachment);
}

void RxTargetList::CreateImages()
{
    for (RxTarget& attachment : Targets) {
        attachment.CreateImage();
    }

    mbImagesCreated = true;
}

FxSizedArray<VkImageView>& RxTargetList::GetImageViews()
{
    // Return the list of views if it is already populated
    if (mbImageViewsBuilt) {
        return mBuiltImageViews;
    }

    if (!mBuiltImageViews.IsInited()) {
        mBuiltImageViews.InitCapacity(Targets.Size);
    }

    mBuiltImageViews.Clear();

    for (RxTarget& attachment : Targets) {
        // Check to ensure that the image (and therefore the view) is created.
        if (!attachment.Image.IsInited()) {
            continue;
        }

        mBuiltImageViews.Insert(attachment.Image.View);
    }

    mbImageViewsBuilt = true;

    return mBuiltImageViews;
}

bool RxTargetList::IsCompatible(const RxTargetList& other) const
{
    if (Targets.Size != other.Targets.Size) {
        return false;
    }

    for (uint32 index = 0; index < Targets.Size; index++) {
        const RxTarget& t = Targets[index];
        const RxTarget& other_t = other.Targets[index];

        const bool format_matches = t.Image.Format == other_t.Image.Format;

        if (!format_matches) {
            return false;
        }
    }

    return true;
}
