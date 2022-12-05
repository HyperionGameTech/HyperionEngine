#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

class Engine;
class Texture;

using renderer::DescriptorSet;

class BindlessStorage
{
public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage &other) = delete;
    BindlessStorage &operator=(const BindlessStorage &other) = delete;
    ~BindlessStorage();

    void Create();
    void Destroy();

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(const Texture *texture);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(ID<Texture> id);
    /*! \brief Mark a resource as having changed, to be queued for update. */
    void MarkResourceChanged(ID<Texture> id);

    /*! \brief Get the index of the sub-descriptor for the given texture.
     * @returns whether the texture was found or not */
    bool GetResourceIndex(ID<Texture> id, UInt *out_index) const;

private:
    FlatSet<ID<Texture>> m_texture_ids;
    
    FixedArray<DescriptorSet *, max_frames_in_flight> m_descriptor_sets;
    std::mutex m_enqueued_resources_mutex;
};

} // namespace hyperion::v2

#endif
