#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererHelpers.hpp>

namespace hyperion {
namespace renderer {

AttachmentRef::AttachmentRef(
    Attachment *attachment,
    LoadOperation load_operation,
    StoreOperation store_operation
) : AttachmentRef(
        attachment,
        std::make_unique<ImageView>(),
        std::make_unique<Sampler>(),
        load_operation,
        store_operation
    )
{
}

AttachmentRef::AttachmentRef(
    Attachment *attachment,
    std::unique_ptr<ImageView> &&image_view,
    std::unique_ptr<Sampler> &&sampler,
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

AttachmentRef::~AttachmentRef()
{
    AssertThrowMsg(
        !m_is_created,
        "Expected render pass attachment reference to not be in `created` state on destructor call"
    );
}

// TODO: move to heleprs

VkAttachmentLoadOp AttachmentRef::ToVkLoadOp(LoadOperation load_operation)
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

VkAttachmentStoreOp AttachmentRef::ToVkStoreOp(StoreOperation store_operation)
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

VkImageLayout AttachmentRef::GetIntermediateLayout() const
{
    return m_attachment->IsDepthAttachment()
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

InternalFormat AttachmentRef::GetFormat() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
    }

    return m_attachment->GetFormat();
}

bool AttachmentRef::IsDepthAttachment() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return false;
    }

    return m_attachment->IsDepthAttachment();
}

VkAttachmentDescription AttachmentRef::GetAttachmentDescription() const
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

VkAttachmentReference AttachmentRef::GetHandle() const
{
    if (!HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    return VkAttachmentReference{
        .attachment = GetBinding(),
        .layout = GetIntermediateLayout()
    };
}

const AttachmentRef *AttachmentRef::IncRef(AttachmentRefInstance &&ins) const
{
    AssertThrow(m_ref_count != nullptr);

    m_ref_count->m_holder_instances.insert(std::move(ins));

    return this;
}

const AttachmentRef *AttachmentRef::DecRef(AttachmentRefInstance &&ins) const
{
    AssertThrow(m_ref_count != nullptr);
    
    m_ref_count->m_holder_instances.erase(ins);

    return this;
}

Result AttachmentRef::Create(Device *device)
{
    AssertThrow(!m_is_created);

    HYPERION_BUBBLE_ERRORS(m_image_view->Create(device, m_attachment->GetImage()));
    HYPERION_BUBBLE_ERRORS(m_sampler->Create(device));

    m_is_created = true;

    HYPERION_RETURN_OK;
}


Result AttachmentRef::Create(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    UInt num_mipmaps,
    UInt num_faces
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

Result AttachmentRef::Destroy(Device *device)
{
    AssertThrow(m_is_created);

    HYPERION_BUBBLE_ERRORS(m_image_view->Destroy(device));
    HYPERION_BUBBLE_ERRORS(m_sampler->Destroy(device));

    m_is_created = false;

    HYPERION_RETURN_OK;
}

Result AttachmentRef::AddAttachmentRef(Device *device, StoreOperation store_operation, AttachmentRef **out)
{
    auto result = Result::OK;

    HYPERION_BUBBLE_ERRORS(result = m_attachment->AddAttachmentRef(device, LoadOperation::LOAD, store_operation, out));

    (*out)->m_initial_layout = m_final_layout;
    (*out)->m_final_layout = m_final_layout;

    return result;
}

Result AttachmentRef::RemoveSelf(Device *device)
{
    return m_attachment->RemoveAttachmentRef(device, this);
}

Attachment::Attachment(std::unique_ptr<Image> &&image, RenderPassStage stage)
    : m_is_created(false),
      m_image(std::move(image)),
      m_stage(stage)
{
}

Attachment::~Attachment()
{
    AssertThrowMsg(!m_is_created, "Attachment must not be in `created` state on destructor call");

    for (SizeType i = 0; i < m_ref_counts.size(); i++) {
        AssertThrowMsg(
            m_ref_counts[i]->m_holder_instances.empty(),
            "Expected ref count at %llu to be zero after decrement but was %llu -- object still in use somewhere else.",
            i,
            m_ref_counts[i]->m_holder_instances.size()
        );
    }
}

Result Attachment::AddAttachmentRef(
    Device *device,
    LoadOperation load_operation,
    StoreOperation store_operation,
    AttachmentRef **out
)
{
    auto attachment_ref = std::make_unique<AttachmentRef>(
        this,
        std::make_unique<ImageView>(),
        std::make_unique<Sampler>(
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
        HYPERION_BUBBLE_ERRORS(attachment_ref->Create(device));
    }
    
    m_ref_counts.push_back(new AttachmentRef::RefCount);
    attachment_ref->m_ref_count = m_ref_counts.back();
    m_attachment_refs.push_back(std::move(attachment_ref));

    if (out != nullptr) {
        *out = m_attachment_refs.back().get();
    }

    HYPERION_RETURN_OK;
}

Result Attachment::AddAttachmentRef(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    UInt num_mipmaps,
    UInt num_faces,
    LoadOperation load_operation,
    StoreOperation store_operation,
    AttachmentRef **out
)
{
    auto attachment_ref = std::make_unique<AttachmentRef>(this, load_operation, store_operation);

    if (out != nullptr) {
        *out = nullptr;
    }

    if (m_is_created) {
        HYPERION_BUBBLE_ERRORS(attachment_ref->Create(
            device,
            image,
            format,
            aspect_flags,
            view_type,
            num_mipmaps,
            num_faces
        ));
    }
    
    m_ref_counts.push_back(new AttachmentRef::RefCount);
    attachment_ref->m_ref_count = m_ref_counts.back();
    m_attachment_refs.push_back(std::move(attachment_ref));

    if (out != nullptr) {
        *out = m_attachment_refs.back().get();
    }

    HYPERION_RETURN_OK;
}

Result Attachment::RemoveAttachmentRef(Device *device, AttachmentRef *attachment_ref)
{
    AssertThrowMsg(attachment_ref != nullptr, "Attachment reference cannot be null");

    const auto it = std::find_if(m_attachment_refs.begin(), m_attachment_refs.end(), [attachment_ref](const auto &item) {
        return item.get() == attachment_ref;
    });

    if (it == m_attachment_refs.end()) {
        return {Result::RENDERER_ERR, "Attachment ref not found"};
    }

    AssertThrowMsg(
        attachment_ref->m_ref_count->m_holder_instances.empty(),
        "Expected ref count to be empty before explicit removal -- still in use somewhere else."
    );

    const auto ref_count_index = it - m_attachment_refs.begin();

    AssertThrow(attachment_ref->m_ref_count == m_ref_counts[ref_count_index]);

    delete m_ref_counts[ref_count_index];
    m_ref_counts.erase(m_ref_counts.begin() + ref_count_index);

    HYPERION_BUBBLE_ERRORS((*it)->Destroy(device));
    m_attachment_refs.erase(it);

    HYPERION_RETURN_OK;
}

Result Attachment::Create(Device *device)
{
    AssertThrow(!m_is_created);
    AssertThrow(m_image != nullptr);

    auto result = Result::OK;

    HYPERION_BUBBLE_ERRORS(m_image->Create(device));

    m_is_created = true;

    for (auto &attachment_ref : m_attachment_refs) {
        HYPERION_PASS_ERRORS(attachment_ref->Create(device), result);
    }

    return result;
}

Result Attachment::Destroy(Device *device)
{
    AssertThrow(m_is_created);
    AssertThrow(m_image != nullptr);

    auto result = Result::OK;

    HYPERION_PASS_ERRORS(m_image->Destroy(device), result);

    m_is_created = false;

    for (auto &attachment_ref : m_attachment_refs) {
        HYPERION_PASS_ERRORS(attachment_ref->Destroy(device), result);
    }

    return result;
}

AttachmentSet::AttachmentSet(RenderPassStage stage, UInt width, UInt height)
    : m_stage(stage),
      m_width(width),
      m_height(height)
{
}

AttachmentSet::~AttachmentSet()
{
    AssertThrowMsg(m_attachments.empty(), "Expected all attachments to be cleared at destructor call");
    AssertThrowMsg(m_attachment_refs.empty(), "Expected all attachment refs to be cleared at destructor call");
}

bool AttachmentSet::Has(UInt binding) const
{
    return std::any_of(m_attachment_refs.begin(), m_attachment_refs.end(), [binding](const auto &it) {
        return it.first == binding;
    });
}

AttachmentRef *AttachmentSet::Get(UInt binding) const
{
    const auto it = std::find_if(m_attachment_refs.begin(), m_attachment_refs.end(), [binding](const auto &it) {
        return it.first == binding;
    });

    if (it == m_attachment_refs.end()) {
        return nullptr;
    }

    return it->second;
}

Result AttachmentSet::Add(Device *device, UInt binding, InternalFormat format)
{
    return Add(device, binding, std::make_unique<FramebufferImage2D>(Extent2D { m_width, m_height }, format, nullptr));
}

Result AttachmentSet::Add(Device *device, UInt binding, std::unique_ptr<Image> &&image)
{
    if (Has(binding)) {
        return {Result::RENDERER_ERR, "Cannot set duplicate bindings"};
    }

    m_attachments.push_back(std::make_unique<Attachment>(
        std::move(image),
        m_stage
    ));

    return Add(device, binding, m_attachments.back().get());
}

Result AttachmentSet::Add(Device *device, UInt binding, Attachment *attachment)
{
    AssertThrow(attachment != nullptr);

    if (Has(binding)) {
        return {Result::RENDERER_ERR, "Cannot set duplicate bindings"};
    }

    AttachmentRef *attachment_ref;

    HYPERION_BUBBLE_ERRORS(attachment->AddAttachmentRef(
        device,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    attachment_ref->SetBinding(binding);

    m_attachment_refs.emplace_back(binding, attachment_ref);

    SortAttachmentRefs();

    HYPERION_RETURN_OK;
}

Result AttachmentSet::Remove(Device *device, UInt binding)
{
    const auto it = std::find_if(m_attachment_refs.begin(), m_attachment_refs.end(), [binding](const auto &it) {
        return it.first == binding;
    });

    if (it == m_attachment_refs.end()) {
        return {Result::RENDERER_ERR, "Cannot remove attachment reference -- binding not found"};
    }

    AssertThrow(it->second != nullptr);

    auto result = it->second->RemoveSelf(device);

    m_attachment_refs.erase(it);

    return result;
}

void AttachmentSet::SortAttachmentRefs()
{
    std::sort(m_attachment_refs.begin(), m_attachment_refs.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
}

Result AttachmentSet::Create(Device *device)
{
    for (auto &attachment : m_attachments) {
        HYPERION_BUBBLE_ERRORS(attachment->Create(device));
    }

    HYPERION_RETURN_OK;
}

Result AttachmentSet::Destroy(Device *device)
{
    for (auto &it : m_attachment_refs) {
        HYPERION_BUBBLE_ERRORS(it.second->RemoveSelf(device));
    }

    m_attachment_refs.clear();

    for (auto &attachment : m_attachments) {
        HYPERION_BUBBLE_ERRORS(attachment->Destroy(device));
    }

    m_attachments.clear();

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion