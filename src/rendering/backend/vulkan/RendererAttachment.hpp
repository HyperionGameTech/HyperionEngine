#ifndef HYPERION_RENDERER_ATTACHMENT_H
#define HYPERION_RENDERER_ATTACHMENT_H

#include <core/Containers.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <math/MathUtil.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <optional>
#include <unordered_set>

namespace hyperion {
namespace renderer {

struct AttachmentUsageInstance
{
    const char *cls;
    void *ptr;

    bool operator==(const AttachmentUsageInstance &other) const
    {
        return ptr == other.ptr;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(cls);
        hc.Add(ptr);

        return hc;
    }
};

} // namespace renderer
} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::renderer::AttachmentUsageInstance);

namespace hyperion {
namespace renderer {

class Attachment;
class AttachmentUsage;

enum class RenderPassStage
{
    NONE,
    PRESENT, /* for presentation on screen */
    SHADER   /* for use as a sampled texture in a shader */
};

enum class LoadOperation
{
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

enum class StoreOperation
{
    UNDEFINED,
    NONE,
    STORE
};

// for making it easier to track holders
#define HYP_ATTACHMENT_USAGE_INSTANCE \
    ::hyperion::renderer::AttachmentUsageInstance { \
        .cls = typeid(*this).name(),     \
        .ptr = static_cast<void *>(this) \
    }

class AttachmentUsage
{
    friend class Attachment;
public:

    AttachmentUsage(
        Attachment *attachment,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentUsage(
        Attachment *attachment,
        ImageViewRef &&image_view,
        SamplerRef &&sampler,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentUsage(const AttachmentUsage &other) = delete;
    AttachmentUsage &operator=(const AttachmentUsage &other) = delete;
    ~AttachmentUsage();

    Attachment *GetAttachment() const { return m_attachment; }

    const ImageViewRef &GetImageView() const { return m_image_view; }
    const SamplerRef &GetSampler() const { return m_sampler; }

    LoadOperation GetLoadOperation() const { return m_load_operation; }
    StoreOperation GetStoreOperation() const { return m_store_operation; }

    UInt GetBinding() const { return m_binding; }
    void SetBinding(UInt binding) { m_binding = binding; }
    bool HasBinding() const { return m_binding != UINT_MAX; }

    InternalFormat GetFormat() const;
    bool IsDepthAttachment() const;

    VkAttachmentDescription GetAttachmentDescription() const;
    VkAttachmentReference GetHandle() const;

    const AttachmentUsage *IncRef(AttachmentUsageInstance &&ins) const;
    const AttachmentUsage *DecRef(AttachmentUsageInstance &&ins) const;
    
    Result Create(Device *device);
    Result Create(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        UInt num_mipmaps,
        UInt num_faces
    );

    Result Destroy(Device *device);

    Result AddAttachmentUsage(
        Device *device,
        StoreOperation store_operation,
        AttachmentUsage **out = nullptr
    );

    Result RemoveSelf(Device *device);

private:
    struct RefCount
    {
        std::unordered_set<AttachmentUsageInstance> m_holder_instances;
    };

    static VkAttachmentLoadOp ToVkLoadOp(LoadOperation);
    static VkAttachmentStoreOp ToVkStoreOp(StoreOperation);
    
    VkImageLayout GetIntermediateLayout() const;

    Attachment *m_attachment;
    ImageViewRef m_image_view;
    SamplerRef m_sampler;
    
    LoadOperation m_load_operation;
    StoreOperation m_store_operation;
    UInt m_binding = MathUtil::MaxSafeValue<UInt>();

    VkImageLayout m_initial_layout, m_final_layout;

    RefCount *m_ref_count = nullptr;
    bool m_is_created = false;
};

class Attachment
{
    friend class AttachmentUsage;

public:
    Attachment(ImageRef &&image, RenderPassStage stage);
    Attachment(const Attachment &other) = delete;
    Attachment &operator=(const Attachment &other) = delete;
    ~Attachment();

    const ImageRef &GetImage() const { return m_image; }

    auto &GetAttachmentUsages() { return m_attachment_usages; }
    const auto &GetAttachmentUsages() const { return m_attachment_usages; }

    InternalFormat GetFormat() const
        { return m_image ? m_image->GetTextureFormat() : InternalFormat::NONE; }

    bool IsDepthAttachment() const
        { return m_image ? m_image->IsDepthStencil() : false; }

    void AddAttachmentUsage(AttachmentUsageRef &attachment_usage_ref);

    Result AddAttachmentUsage(
        Device *device,
        LoadOperation load_operation,
        StoreOperation store_operation,
        AttachmentUsage **out = nullptr
    );

    Result AddAttachmentUsage(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        UInt num_mipmaps,
        UInt num_faces,
        LoadOperation load_operation,
        StoreOperation store_operation,
        AttachmentUsage **out = nullptr
    );

    Result RemoveAttachmentUsage(Device *device, AttachmentUsage *attachment_usage);

    Result Create(Device *device);
    Result Destroy(Device *device);

private:

    VkImageLayout GetInitialLayout() const
        { return VK_IMAGE_LAYOUT_UNDEFINED; }

    VkImageLayout GetFinalLayout() const
    {
        switch (m_stage) {
        case RenderPassStage::NONE:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case RenderPassStage::PRESENT:
            return IsDepthAttachment()
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case RenderPassStage::SHADER:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        default:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    bool m_is_created;
    ImageRef m_image;
    RenderPassStage m_stage;

    Array<AttachmentUsageRef> m_attachment_usages;
    Array<AttachmentUsage::RefCount *> m_ref_counts;
};

class AttachmentSet
{
public:
    AttachmentSet(RenderPassStage stage, UInt width, UInt height);
    AttachmentSet(const AttachmentSet &other) = delete;
    AttachmentSet &operator=(const AttachmentSet &other) = delete;
    ~AttachmentSet();

    UInt GetWidth() const { return m_width; }
    UInt GetHeight() const { return m_height; }
    RenderPassStage GetStage() const { return m_stage; }

    bool Has(UInt binding) const;

    AttachmentUsage *Get(UInt binding) const;

    /*! \brief Add a new owned attachment, constructed using the width/height provided to this class,
     * along with the given format argument.
     * @param binding The input attachment binding the attachment will take on
     * @param format The image format of the newly constructed Image
     */
    Result Add(Device *device, uint32_t binding, InternalFormat format);
    /*! \brief Add a new owned attachment using the given image argument.
     * @param binding The input attachment binding the attachment will take on
     * @param image The unique pointer to a non-initialized (but constructed)
     * Image which will be used to render to for this attachment.
     */
    Result Add(Device *device, UInt binding, ImageRef &&image);

    /*! \brief Add a reference to an existing attachment, not owned.
     * An AttachmentUsage is created and its reference count incremented.
     * @param binding The input attachment binding the attachment will take on
     * @param attachment Pointer to an Attachment that exists elsewhere and will be used
     * as an input for this set of attachments. The operation will be an OP_LOAD.
     */
    Result Add(Device *device, UInt binding, Attachment *attachment);

    /*! \brief Remove an attachment reference by the binding argument */
    Result Remove(Device *device, UInt binding);

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    void SortAttachmentUsages();

    UInt m_width, m_height;
    RenderPassStage m_stage;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    std::vector<std::pair<UInt, AttachmentUsage *>> m_attachment_usages;
};

} // namespace renderer
} // namespace hyperion

#endif
