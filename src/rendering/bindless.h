#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include "base.h"
#include <core/containers.h>
#include <core/lib/flat_set.h>
#include <constants.h>

#include <rendering/backend/renderer_descriptor_set.h>
#include <rendering/backend/renderer_swapchain.h>

#include <queue>
#include <array>
#include <mutex>
#include <atomic>

namespace hyperion::v2 {

class Engine;
class Texture;

using renderer::DescriptorSet;
using renderer::Swapchain;

class BindlessStorage {
    /* The index of the descriptor we work on, /within/ the "bindless descriptor set" */
    static constexpr uint32_t bindless_descriptor_index = 0;

public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage &other) = delete;
    BindlessStorage &operator=(const BindlessStorage &other) = delete;
    ~BindlessStorage();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*! \brief Apply changes to the bindless descriptor set corresponding to the given frame index value.
     * Do not call this with the index of a frame that is still using resources.
     */
    void ApplyUpdates(Engine *engine, uint32_t frame_index);

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(const Texture *texture);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(const Texture *texture);
    /*! \brief Mark a resource as having changed, to be queued for update. */
    void MarkResourceChanged(const Texture *texture);

    /*! \brief Get the index of the sub-descriptor for the given texture.
     * @returns whether the texture was found or not */
    bool GetResourceIndex(const Texture *texture, uint32_t *out_index) const;

private:
    struct TextureResource {
        const Texture *texture;
        uint32_t       resource_index;
    };

    FlatSet<IDBase> m_texture_ids;
    
    std::array<DescriptorSet *, max_frames_in_flight> m_descriptor_sets;
    std::mutex m_enqueued_resources_mutex;
};

} // namespace hyperion::v2

#endif
