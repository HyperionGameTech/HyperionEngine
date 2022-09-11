#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/FlatSet.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererSwapchain.hpp>

#include <queue>
#include <array>
#include <mutex>
#include <atomic>

namespace hyperion::v2 {

class Engine;
class Texture;

using renderer::DescriptorSet;
using renderer::Swapchain;

class BindlessStorage
{
    /* The index of the descriptor we work on, /within/ the "bindless descriptor set" */
    static constexpr UInt bindless_descriptor_index = 0;

public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage &other) = delete;
    BindlessStorage &operator=(const BindlessStorage &other) = delete;
    ~BindlessStorage();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(const Texture *texture);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(IDBase id);
    /*! \brief Mark a resource as having changed, to be queued for update. */
    void MarkResourceChanged(const Texture *texture);

    /*! \brief Get the index of the sub-descriptor for the given texture.
     * @returns whether the texture was found or not */
    bool GetResourceIndex(const Texture *texture, UInt *out_index) const;

private:
    struct TextureResource
    {
        const Texture *texture;
        UInt resource_index;
    };

    FlatSet<IDBase> m_texture_ids;
    
    FixedArray<DescriptorSet *, max_frames_in_flight> m_descriptor_sets;
    std::mutex m_enqueued_resources_mutex;
};

} // namespace hyperion::v2

#endif
