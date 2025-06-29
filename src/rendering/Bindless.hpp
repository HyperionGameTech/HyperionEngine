/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BINDLESS_HPP
#define HYPERION_BINDLESS_HPP

#include <core/Base.hpp>
#include <core/Id.hpp>

#include <core/containers/HashMap.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class Engine;
class Texture;

class BindlessStorage
{
public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage& other) = delete;
    BindlessStorage& operator=(const BindlessStorage& other) = delete;
    BindlessStorage(BindlessStorage&& other) noexcept = delete;
    BindlessStorage& operator=(BindlessStorage&& other) noexcept = delete;
    ~BindlessStorage();

    void Create();
    void Destroy();

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(Id<Texture> id, const ImageViewRef& image_view);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(Id<Texture> id);

private:
    HashMap<Id<Texture>, ImageViewWeakRef> m_resources;
};

} // namespace hyperion

#endif
