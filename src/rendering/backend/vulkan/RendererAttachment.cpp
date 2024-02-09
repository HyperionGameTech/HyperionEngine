#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererHelpers.hpp>

namespace hyperion {
namespace renderer {

AttachmentUsage::AttachmentUsage(
    Attachment *attachment,
    LoadOperation load_operation,
    StoreOperation store_operation
) : AttachmentUsage(
        attachment,
        MakeRenderObject<ImageView, Platform::VULKAN>(),
        MakeRenderObject<Sampler, Platform::VULKAN>(),
        load_operation,
        store_operation
    )
{
}

AttachmentUsage::AttachmentUsage(
    Attachment *attachment,
    ImageViewRef &&image_view,
    SamplerRef &&sampler,
    LoadOperation load_operation,
    StoreOperation store_operation
) : m_attachment(attachment),
    m_load_operation(load_operation),
    m_store_operation(store_operation),
    m_initial_layout(attachment->GetInitialLayout()),
    m_final_layout(attachment->GetFinalLayout()),
    m_image_view(std::move(image_view)),
    m_sampler(std::move(sampler))
{
}

AttachmentUsage::~AttachmentUsage()
{
    AssertThrowMsg(
        !m_is_created,
        "Expected render pass attachment reference to not be in `created` state on destructor call"
    );
}

// TODO: move to heleprs

VkAttachmentLoadOp AttachmentUsage::ToVkLoadOp(LoadOperation load_operation)
{
    switch (load_operation) {
    case LoadOperation::UNDEFINED:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::NONE:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    case LoadOperation::CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    default:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp AttachmentUsage::ToVkStoreOp(StoreOperation store_operation)
{
    switch (store_operation) {
    case StoreOperation::UNDEFINED:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::NONE:
        return VK_ATTACHMENT_STORE_OP_NONE_EXT;
    case StoreOperation::STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

VkImageLayout AttachmentUsage::GetIntermediateLayout() const
{
    return m_attachment->IsDepthAttachment()
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

InternalFormat AttachmentUsage::GetFormat() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return InternalFormat::NONE;
    }

    return m_attachment->GetFormat();
}

bool AttachmentUsage::IsDepthAttachment() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return false;
    }

    return m_attachment->IsDepthAttachment();
}

VkAttachmentDescription AttachmentUsage::GetAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format = helpers::ToVkFormat(m_attachment->GetFormat()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = ToVkLoadOp(m_load_operation),
        .storeOp = ToVkStoreOp(m_store_operation),
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = m_initial_layout,
        .finalLayout = m_final_layout
    };
}

VkAttachmentReference AttachmentUsage::GetHandle() const
{
    if (!HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    return VkAttachmentReference{
        .attachment = GetBinding(),
        .layout = GetIntermediateLayout()
    };
}

const AttachmentUsage *AttachmentUsage::IncRef(AttachmentUsageInstance &&ins) const
{
    AssertThrow(m_ref_count != nullptr);

    m_ref_count->m_holder_instances.insert(std::move(ins));

    return this;
}

const AttachmentUsage *AttachmentUsage::DecRef(AttachmentUsageInstance &&ins) const
{
    AssertThrow(m_ref_count != nullptr);
    
    m_ref_count->m_holder_instances.erase(ins);

    return this;
}

Result AttachmentUsage::Create(Device *device)
{
    AssertThrow(!m_is_created);

    HYPERION_BUBBLE_ERRORS(m_image_view->Create(device, m_attachment->GetImage()));
    HYPERION_BUBBLE_ERRORS(m_sampler->Create(device));

    m_is_created = true;

    HYPERION_RETURN_OK;
}


Result AttachmentUsage::Create(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    uint num_mipmaps,
    uint num_faces
)
{
    AssertThrow(!m_is_created);
    
    HYPERION_BUBBLE_ERRORS(m_image_view->Create(
        device,
        image,
        format,
        aspect_flags,
        view_type,
        0,
        num_mipmaps,
        0,
        num_faces
    ));

    HYPERION_BUBBLE_ERRORS(m_sampler->Create(device));

    m_is_created = true;

    HYPERION_RETURN_OK;
}

Result AttachmentUsage::Destroy(Device *device)
{
    AssertThrow(m_is_created);

    SafeRelease(std::move(m_image_view));
    SafeRelease(std::move(m_sampler));

    m_is_created = false;

    HYPERION_RETURN_OK;
}

Result AttachmentUsage::AddAttachmentUsage(Device *device, StoreOperation store_operation, AttachmentUsage **out)
{
    auto result = Result::OK;

    HYPERION_BUBBLE_ERRORS(result = m_attachment->AddAttachmentUsage(device, LoadOperation::LOAD, store_operation, out));

    (*out)->m_initial_layout = m_final_layout;
    (*out)->m_final_layout = m_final_layout;

    return result;
}

Result AttachmentUsage::RemoveSelf(Device *device)
{
    return m_attachment->RemoveAttachmentUsage(device, this);
}

Attachment::Attachment(ImageRef_VULKAN &&image, RenderPassStage stage)
    : m_is_created(false),
      m_image(std::move(image)),
      m_stage(stage)
{
}

Attachment::~Attachment()
{
    AssertThrowMsg(!m_is_created, "Attachment must not be in `created` state on destructor call");

    for (SizeType i = 0; i < m_ref_counts.Size(); i++) {
        AssertThrowMsg(
            m_ref_counts[i]->m_holder_instances.empty(),
            "Expected ref count at %llu to be zero after decrement but was %llu -- object still in use somewhere else.",
            i,
            m_ref_counts[i]->m_holder_instances.size()
        );
    }
}

void Attachment::AddAttachmentUsage(AttachmentUsageRef &attachment_usage_ref)
{
    AssertThrow(attachment_usage_ref.IsValid());
    AssertThrow(attachment_usage_ref->m_attachment == this);
    AssertThrow(attachment_usage_ref->m_ref_count == nullptr);

    m_ref_counts.PushBack(new AttachmentUsage::RefCount);
    attachment_usage_ref->m_ref_count = m_ref_counts.Back();

    m_attachment_usages.PushBack(attachment_usage_ref);
}

Result Attachment::AddAttachmentUsage(
    Device *device,
    LoadOperation load_operation,
    StoreOperation store_operation,
    AttachmentUsage **out
)
{
    auto attachment_usage = MakeRenderObject<AttachmentUsage, Platform::VULKAN>(
        this,
        MakeRenderObject<ImageView, Platform::VULKAN>(),
        MakeRenderObject<Sampler, Platform::VULKAN>(
            m_image != nullptr
                ? m_image->GetFilterMode()
                : FilterMode::TEXTURE_FILTER_NEAREST
        ),
        load_operation,
        store_operation
    );

    if (out != nullptr) {
        *out = nullptr;
    }

    if (m_is_created) {
        HYPERION_BUBBLE_ERRORS(attachment_usage->Create(device));
    }
    
    m_ref_counts.PushBack(new AttachmentUsage::RefCount);
    attachment_usage->m_ref_count = m_ref_counts.Back();
    m_attachment_usages.PushBack(std::move(attachment_usage));

    if (out != nullptr) {
        *out = m_attachment_usages.Back().Get();
    }

    HYPERION_RETURN_OK;
}

Result Attachment::AddAttachmentUsage(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    uint num_mipmaps,
    uint num_faces,
    LoadOperation load_operation,
    StoreOperation store_operation,
    AttachmentUsage **out
)
{
    auto attachment_usage = MakeRenderObject<AttachmentUsage, Platform::VULKAN>(this, load_operation, store_operation);

    if (out != nullptr) {
        *out = nullptr;
    }

    if (m_is_created) {
        HYPERION_BUBBLE_ERRORS(attachment_usage->Create(
            device,
            image,
            format,
            aspect_flags,
            view_type,
            num_mipmaps,
            num_faces
        ));
    }
    
    m_ref_counts.PushBack(new AttachmentUsage::RefCount);
    attachment_usage->m_ref_count = m_ref_counts.Back();
    m_attachment_usages.PushBack(std::move(attachment_usage));

    if (out != nullptr) {
        *out = m_attachment_usages.Back().Get();
    }

    HYPERION_RETURN_OK;
}

Result Attachment::RemoveAttachmentUsage(Device *device, AttachmentUsage *attachment_usage)
{
    AssertThrowMsg(attachment_usage != nullptr, "Attachment reference cannot be null");

    const auto it = m_attachment_usages.FindIf([attachment_usage](const auto &item) {
        return item.Get() == attachment_usage;
    });

    if (it == m_attachment_usages.End()) {
        return { Result::RENDERER_ERR, "Attachment ref not found" };
    }

    AssertThrowMsg(
        attachment_usage->m_ref_count->m_holder_instances.empty(),
        "Expected ref count to be empty before explicit removal -- still in use somewhere else."
    );

    const auto ref_count_index = it - m_attachment_usages.Begin();

    AssertThrow(attachment_usage->m_ref_count == m_ref_counts[ref_count_index]);

    delete m_ref_counts[ref_count_index];
    m_ref_counts.Erase(m_ref_counts.Begin() + ref_count_index);

    HYPERION_BUBBLE_ERRORS((*it)->Destroy(device));
    m_attachment_usages.Erase(it);

    HYPERION_RETURN_OK;
}

Result Attachment::Create(Device *device)
{
    AssertThrow(!m_is_created);
    AssertThrow(m_image != nullptr);

    auto result = Result::OK;

    HYPERION_BUBBLE_ERRORS(m_image->Create(device));

    m_is_created = true;

    for (auto &attachment_usage : m_attachment_usages) {
        HYPERION_PASS_ERRORS(attachment_usage->Create(device), result);
    }

    return result;
}

Result Attachment::Destroy(Device *device)
{
    AssertThrow(m_is_created);
    AssertThrow(m_image != nullptr);

    auto result = Result::OK;

    SafeRelease(std::move(m_image));

    m_is_created = false;

    for (auto &attachment_usage : m_attachment_usages) {
        HYPERION_PASS_ERRORS(attachment_usage->Destroy(device), result);
    }

    return result;
}

} // namespace renderer
} // namespace hyperion