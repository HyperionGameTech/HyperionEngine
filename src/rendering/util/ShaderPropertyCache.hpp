/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace hyperion {

/*! \brief The purpose of ShaderPropertyCache is to assign all ShaderProperty hashes to an
 *  index in a contiguous array upon first seeing it and then reusing that index for
 *  subsequent uses of the same ShaderProperty. This allows us to use bitsets to represent
 *  sets of ShaderProperties efficiently, rather than using HashSets (which are used for serializing and deserializing them). */

enum class ShaderPropertyId : uint32;

struct ShaderProperty;

class ShaderPropertyCache
{
public:
    ShaderPropertyCache();

    ShaderPropertyCache(const ShaderPropertyCache& other) = delete;
    ShaderPropertyCache& operator=(const ShaderPropertyCache& other) = delete;

    ShaderPropertyCache(ShaderPropertyCache&& other) noexcept = delete;
    ShaderPropertyCache& operator=(ShaderPropertyCache&& other) noexcept = delete;

    ~ShaderPropertyCache();

    /*! \brief Returns the assigned index for the given property, assigning a new index if it does not exist yet. */
    ShaderPropertyId GetOrAssignIndex(const ShaderProperty& property);

    /*! \brief Returns the ShaderProperty assigned to the given id, or nullptr if not found. */
    const ShaderProperty* GetPropertyById(ShaderPropertyId id) const;

private:
    struct ShaderPropertyCacheImpl* m_pImpl;
};

} // namespace hyperion
