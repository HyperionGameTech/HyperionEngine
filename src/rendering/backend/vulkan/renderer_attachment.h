#ifndef HYPERION_RENDERER_ATTACHMENT_H
#define HYPERION_RENDERER_ATTACHMENT_H

#include "renderer_device.h"
#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"

#include <hash_code.h>
#include <types.h>

#include <vulkan/vulkan.h>

#include <optional>
#include <unordered_set>

namespace hyperion {
namespace renderer {

struct AttachmentRefInstance {
    const char *cls;
    void       *ptr;

    bool operator==(const AttachmentRefInstance &other) const
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

HYP_DEF_STL_HASH(hyperion::renderer::AttachmentRefInstance);

namespace hyperion {
namespace renderer {

class Attachment;
class AttachmentRef;

enum class RenderPassStage {
    NONE,
    PRESENT, /* for presentation on screen */
    SHADER   /* for use as a sampled texture in a shader */
};

enum class LoadOperation {
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

enum class StoreOperation {
    UNDEFINED,
    NONE,
    STORE
};

// for making it easier to track holders
#define HYP_ATTACHMENT_REF_INSTANCE \
    ::hyperion::renderer::AttachmentRefInstance{ \
        .cls = typeid(*this).name(),     \
        .ptr = static_cast<void *>(this) \
    }

class AttachmentRef {
    friend class Attachment;
public:

    AttachmentRef(
        Attachment *attachment,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentRef(
        Attachment *attachment,
        std::unique_ptr<ImageView> &&image_view,
        std::unique_ptr<Sampler> &&sampler,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE
    );

    AttachmentRef(const AttachmentRef &other) = delete;
    AttachmentRef &operator=(const AttachmentRef &other) = delete;
    ~AttachmentRef();

    inline Attachment *GetAttachment() const        { return m_attachment; }

    inline ImageView *GetImageView() const          { return m_image_view.get(); }
    inline Sampler *GetSampler() const              { return m_sampler.get(); }

    inline LoadOperation GetLoadOperation() const   { return m_load_operation; }
    inline StoreOperation GetStoreOperation() const { return m_store_operation; }

    inline uint GetBinding() const                  { return m_binding.value_or(UINT32_MAX); }
    inline void SetBinding(uint binding)            { m_binding = binding; }
    inline bool HasBinding() const                  { return m_binding.has_value(); }

    Image::InternalFormat GetFormat() const;
    bool IsDepthAttachment() const;

    VkAttachmentDescription GetAttachmentDescription() const;
    VkAttachmentReference GetHandle() const;

    void IncRef(AttachmentRefInstance &&ins) const;
    void DecRef(AttachmentRefInstance &&ins) const;
    
    Result Create(Device *device);
    Result Create(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        size_t num_mipmaps,
        size_t num_faces
    );

    Result Destroy(Device *device);

    Result AddAttachmentRef(
        Device *device,
        StoreOperation store_operation,
        AttachmentRef **out = nullptr
    );

    Result RemoveSelf(Device *device);

private:
    struct RefCount {
        //uint count = 0;

        std::unordered_set<AttachmentRefInstance> m_holder_instances;
    };

    static VkAttachmentLoadOp ToVkLoadOp(LoadOperation);
    static VkAttachmentStoreOp ToVkStoreOp(StoreOperation);
    
    VkImageLayout GetIntermediateLayout() const;

    Attachment *m_attachment;
    std::unique_ptr<ImageView> m_image_view;
    std::unique_ptr<Sampler> m_sampler;
    
    LoadOperation m_load_operation;
    StoreOperation m_store_operation;
    std::optional<uint> m_binding{};

    VkImageLayout m_initial_layout, m_final_layout;

    RefCount *m_ref_count = nullptr;
    bool m_is_created     = false;
};

class Attachment {
    friend class AttachmentRef;

public:
    Attachment(std::unique_ptr<Image> &&image, RenderPassStage stage);
    Attachment(const Attachment &other) = delete;
    Attachment &operator=(const Attachment &other) = delete;
    ~Attachment();

    inline Image *GetImage() const { return m_image.get(); }

    inline auto &GetAttachmentRefs()             { return m_attachment_refs; }
    inline const auto &GetAttachmentRefs() const { return m_attachment_refs; }

    inline Image::InternalFormat GetFormat() const
        { return m_image ? m_image->GetTextureFormat() : Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE; }

    inline bool IsDepthAttachment() const
        { return m_image ? m_image->IsDepthStencil() : false; }

    Result AddAttachmentRef(
        Device *device,
        LoadOperation load_operation,
        StoreOperation store_operation,
        AttachmentRef **out = nullptr
    );

    Result AddAttachmentRef(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        size_t num_mipmaps,
        size_t num_faces,
        LoadOperation load_operation,
        StoreOperation store_operation,
        AttachmentRef **out = nullptr
    );

    Result RemoveAttachmentRef(Device *device, AttachmentRef *attachment_ref);

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
    std::unique_ptr<Image> m_image;
    RenderPassStage m_stage;

    std::vector<std::unique_ptr<AttachmentRef>> m_attachment_refs;
    std::vector<AttachmentRef::RefCount *> m_ref_counts;
};

class AttachmentSet {
public:
    AttachmentSet(RenderPassStage stage, size_t width, size_t height);
    AttachmentSet(const AttachmentSet &other) = delete;
    AttachmentSet &operator=(const AttachmentSet &other) = delete;
    ~AttachmentSet();

    inline size_t GetWidth() const { return m_width; }
    inline size_t GetHeight() const { return m_height; }
    inline RenderPassStage GetStage() const { return m_stage; }

    bool Has(uint32_t binding) const;

    AttachmentRef *Get(uint32_t binding) const;

    /*! \brief Add a new owned attachment, constructed using the width/height provided to this class,
     * along with the given format argument.
     * @param binding The input attachment binding the attachment will take on
     * @param format The image format of the newly constructed Image
     */
    Result Add(Device *device, uint32_t binding, Image::InternalFormat format);
    /*! \brief Add a new owned attachment using the given image argument.
     * @param binding The input attachment binding the attachment will take on
     * @param image The unique pointer to a non-initialized (but constructed)
     * Image which will be used to render to for this attachment.
     */
    Result Add(Device *device, uint32_t binding, std::unique_ptr<Image> &&image);

    /*! \brief Add a reference to an existing attachment, not owned.
     * An AttachmentRef is created and its reference count incremented.
     * @param binding The input attachment binding the attachment will take on
     * @param attachment Pointer to an Attachment that exists elsewhere and will be used
     * as an input for this set of attachments. The operation will be an OP_LOAD.
     */
    Result Add(Device *device, uint32_t binding, Attachment *attachment);

    /*! \brief Remove an attachment reference by the binding argument */
    Result Remove(Device *device, uint32_t binding);

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    void SortAttachmentRefs();

    size_t m_width, m_height;
    RenderPassStage m_stage;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    std::vector<std::pair<uint32_t, AttachmentRef *>> m_attachment_refs;
};

} // namespace renderer
} // namespace hyperion

#endif
