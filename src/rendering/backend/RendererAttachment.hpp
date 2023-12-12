#ifndef HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H
#define HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H

#include <core/Containers.hpp>

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

struct AttachmentUsageInstance
{
    const char  *cls;
    void        *ptr;

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


namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Instance;

} // namespace platform

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

namespace platform {

// for making it easier to track holders
#define HYP_ATTACHMENT_USAGE_INSTANCE \
    ::hyperion::renderer::AttachmentUsageInstance { \
        .cls = typeid(*this).name(),     \
        .ptr = static_cast<void *>(this) \
    }

template <PlatformType PLATFORM>
class AttachmentSet
{
};

template <PlatformType PLATFORM>
class AttachmentUsage
{
};

template <PlatformType PLATFORM>
class Attachment
{
};

} // namespace platform

} // namespace renderer
} // namespace hyperion


#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererAttachment.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using AttachmentSet     = platform::AttachmentSet<Platform::CURRENT>;
using AttachmentUsage   = platform::AttachmentUsage<Platform::CURRENT>;
using Attachment        = platform::Attachment<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif