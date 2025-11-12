/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjId.hpp>
#include <core/reflection/Handle.hpp>

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

    /*! \brief Add a texture to the bindless descriptor set.
     *  \param boundIndex The current bound index for the texture. \see ResourceBindings.cpp
     *  \param texture The texture to add
     */
    void AddResource(uint32 boundIndex, Texture* texture);

    /*! \brief Remove the texture at the given bound index from the bindless descriptor set.
     *  \param boundIndex The bound index of the texture to remove.
     */
    void RemoveResource(uint32 boundIndex);

private:
    HashMap<uint32, WeakHandle<Texture>> m_textures;
};

} // namespace hyperion
