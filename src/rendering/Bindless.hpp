/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <core/lib/Mutex.hpp>
#include <Constants.hpp>

namespace hyperion::v2 {

class Engine;
class Texture;

class BindlessStorage
{
public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage &other)                   = delete;
    BindlessStorage &operator=(const BindlessStorage &other)        = delete;
    BindlessStorage(BindlessStorage &&other) noexcept               = delete;
    BindlessStorage &operator=(BindlessStorage &&other) noexcept    = delete;
    ~BindlessStorage();

    void Create();
    void Destroy();

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(ID<Texture> id, ImageViewRef image_view);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(ID<Texture> id);

private:
    HashMap<ID<Texture>, ImageViewWeakRef>  m_resources;
};

} // namespace hyperion::v2

#endif
