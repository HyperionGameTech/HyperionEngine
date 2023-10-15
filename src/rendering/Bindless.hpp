#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <core/lib/Mutex.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

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
    void AddResource(Texture *texture);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(ID<Texture> id);
    /*! \brief Mark a resource as having changed, to be queued for update. */
    void MarkResourceChanged(ID<Texture> id);
    
    Texture *GetResource(ID<Texture> id) const;

private:
    FlatMap<ID<Texture>, Texture *>                     m_texture_ids;
    FixedArray<DescriptorSetRef, max_frames_in_flight>  m_descriptor_sets;
};

} // namespace hyperion::v2

#endif
