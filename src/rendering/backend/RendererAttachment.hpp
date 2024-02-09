#ifndef HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H
#define HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H

#include <util/Defines.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/Platform.hpp>
#include <math/Extent.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Instance;

} // namespace platform

using Device    = platform::Device<Platform::CURRENT>;
using Instance  = platform::Instance<Platform::CURRENT>;

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

class AttachmentSet
{
public:
    AttachmentSet(RenderPassStage stage, Extent3D extent);
    AttachmentSet(const AttachmentSet &other) = delete;
    AttachmentSet &operator=(const AttachmentSet &other) = delete;
    ~AttachmentSet();

    Extent3D GetExtent() const
        { return m_extent; }

    RenderPassStage GetStage() const
        { return m_stage; }

    bool Has(uint binding) const;

    AttachmentUsage *Get(uint binding) const;

    /*! \brief Add a new owned attachment, constructed using the width/height provided to this class,
     * along with the given format argument.
     * @param binding The input attachment binding the attachment will take on
     * @param format The image format of the newly constructed Image
     */
    Result Add(Device *device, uint binding, InternalFormat format);
    /*! \brief Add a new owned attachment using the given image argument.
     * @param binding The input attachment binding the attachment will take on
     * @param image The unique pointer to a non-initialized (but constructed)
     * Image which will be used to render to for this attachment.
     */
    Result Add(Device *device, uint binding, ImageRef &&image);

    /*! \brief Add a reference to an existing attachment, not owned.
     * An AttachmentUsage is created and its reference count incremented.
     * @param binding The input attachment binding the attachment will take on
     * @param attachment Pointer to an Attachment that exists elsewhere and will be used
     * as an input for this set of attachments. The operation will be an OP_LOAD.
     */
    Result Add(Device *device, uint binding, AttachmentRef attachment);

    /*! \brief Remove an attachment reference by the binding argument */
    Result Remove(Device *device, uint binding);

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    Extent3D                            m_extent;
    RenderPassStage                     m_stage;
    Array<AttachmentRef>                m_attachments;
    FlatMap<uint, AttachmentUsage *>    m_attachment_usages;
};

} // namespace renderer
} // namespace hyperion


#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererAttachment.hpp>
#else
#error Unsupported rendering backend
#endif

#endif