/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BINDLESS_HPP
#define HYPERION_BINDLESS_HPP

#include <core/Base.hpp>
#include <core/ID.hpp>

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
    void AddResource(ID<Texture> id, const ImageViewRef& image_view);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(ID<Texture> id);

private:
    HashMap<ID<Texture>, ImageViewWeakRef> m_resources;
};

} // namespace hyperion

#endif
