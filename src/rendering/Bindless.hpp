/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>

#include <core/containers/HashMap.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

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

    void UnsetAllResources();

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(ObjId<Texture> id, const GpuImageViewRef& imageView);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(ObjId<Texture> id);

private:
    HashMap<ObjId<Texture>, GpuImageViewWeakRef> m_resources;
};

} // namespace hyperion
