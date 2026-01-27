/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/memory/Pimpl.hpp>

#include <rendering/RenderObject.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {

struct ShaderDefinition;
class ShaderVariant;

enum class ShaderCacheId : uint64;
static constexpr ShaderCacheId InvalidShaderCacheId = ShaderCacheId(0);

class ShaderManager
{
public:
    static ShaderManager* GetInstance();

    ShaderManager();
    
    ShaderRef GetOrCreate(
        Name name,
        const ShaderPropertySet& properties,
        const VertexAttributeSet& vertexAttributes);

    SizeType CalculateMemoryUsage() const;

private:
    /*! \brief Gets a unique ShaderCacheId for the given ShaderDefinition.
     *  If \p createIfNotExists is true, a new ShaderCacheId will be created
     *  and stored if one does not already exist for the given definition. (The shader will be created on demand when requested.)
     *  Otherwise, if no ShaderCacheId exists for the definition, an invalid ShaderCacheId will be returned.
     *  A ShaderCacheId is a unique identifier for a ShaderDefinition, used
     *  with RuntimeMaterialAttributes to reference shaders without holding
     *  a full ShaderDefinition object.
     */
    ShaderCacheId GetShaderCacheId(
        Name name,
        const ShaderPropertySet& properties,
        const VertexAttributeSet& vertexAttributes,
        bool createIfNotExists = true) const;

    Pimpl<class ShaderManagerImpl> m_impl;
};

} // namespace Hyperion
