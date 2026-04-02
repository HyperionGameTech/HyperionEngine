/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/name/Name.hpp>

#include <Core/memory/Pimpl.hpp>

#include <rendering/RenderObject.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {

enum class ShaderCacheId : uint64;
static constexpr ShaderCacheId InvalidShaderCacheId = ShaderCacheId(0);

class ShaderManager
{
public:
    ShaderManager();
    
    ShaderInstanceRef GetOrCreate(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout);

    size_t CalculateMemoryUsage() const;

private:
    /*! \brief Gets a unique ShaderCacheId for the given shader info.
    *   If the shader has already been loaded or if this method has been called before,
    *   the same ShaderCacheId will be returned.
    *   However, this value is not persistent across runs.
    */
    ShaderCacheId GetShaderCacheId(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        bool createIfNotExists = true) const;

    Pimpl<class ShaderManagerImpl> m_impl;
};

} // namespace Hyperion
