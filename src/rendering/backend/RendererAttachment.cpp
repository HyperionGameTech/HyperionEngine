#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <system/Debug.hpp>

namespace hyperion::renderer {

AttachmentSet::AttachmentSet(RenderPassStage stage, Extent3D extent)
    : m_stage(stage),
      m_extent(extent)
{
}

AttachmentSet::~AttachmentSet()
{
    AssertThrowMsg(m_attachments.Empty(), "Expected all attachments to be cleared at destructor call");
    AssertThrowMsg(m_attachment_usages.Empty(), "Expected all attachment refs to be cleared at destructor call");
}

bool AttachmentSet::Has(UInt binding) const
{
    const auto it = m_attachment_usages.FindIf([binding](const auto &it) { return it.first == binding; });
    
    return it != m_attachment_usages.End();
}

AttachmentUsage *AttachmentSet::Get(UInt binding) const
{
    const auto it = m_attachment_usages.FindIf([binding](const auto &it) { return it.first == binding; });

    if (it == m_attachment_usages.End()) {
        return nullptr;
    }

    return it->second;
}

Result AttachmentSet::Add(Device *device, UInt binding, InternalFormat format)
{
    return Add(device, binding, RenderObjects::Make<Image>(FramebufferImage2D(Extent2D(m_extent), format, nullptr)));
}

Result AttachmentSet::Add(Device *device, UInt binding, ImageRef &&image)
{
    AssertThrow(image.IsValid());

    if (Has(binding)) {
        return { Result::RENDERER_ERR, "Cannot set duplicate bindings" };
    }

    m_attachments.PushBack(RenderObjects::Make<Attachment>(
        std::move(image),
        m_stage
    ));

    return Add(device, binding, m_attachments.Back());
}

Result AttachmentSet::Add(Device *device, UInt binding, AttachmentRef attachment)
{
    AssertThrow(attachment != nullptr);

    if (Has(binding)) {
        return { Result::RENDERER_ERR, "Cannot set duplicate bindings" };
    }

    AttachmentUsage *attachment_usage;

    HYPERION_BUBBLE_ERRORS(attachment->AddAttachmentUsage(
        device,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    attachment_usage->SetBinding(binding);

    m_attachment_usages.Insert(binding, attachment_usage);
    m_attachments.PushBack(std::move(attachment));

    HYPERION_RETURN_OK;
}

Result AttachmentSet::Remove(Device *device, UInt binding)
{
    const auto it = std::find_if(m_attachment_usages.begin(), m_attachment_usages.end(), [binding](const auto &it) {
        return it.first == binding;
    });

    if (it == m_attachment_usages.end()) {
        return { Result::RENDERER_ERR, "Cannot remove attachment reference -- binding not found" };
    }

    AssertThrow(it->second != nullptr);

    auto result = it->second->RemoveSelf(device);

    m_attachment_usages.Erase(it);

    return result;
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
    for (auto &it : m_attachment_usages) {
        HYPERION_BUBBLE_ERRORS(it.second->RemoveSelf(device));
    }

    m_attachment_usages.Clear();

    SafeRelease(std::move(m_attachments));

    HYPERION_RETURN_OK;
}

} // namespace hyperion::renderer