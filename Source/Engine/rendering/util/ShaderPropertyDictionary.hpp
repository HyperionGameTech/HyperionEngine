/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/serialization/BinaryDictionary.hpp>

namespace Hyperion {

enum class ShaderPropertyId : uint32;

struct ShaderProperty;

class ByteWriter;
class ByteReader;

/*! \brief ShaderPropertyDictionary assigns all ShaderProperty hashes to a contiguous index
 *  upon first use and reuses that index on subsequent encounters of the same ShaderProperty.
 *  This allows sets of ShaderProperties to be represented as efficient bitsets rather than
 *  HashSets (which are used only for serialization and deserialization) */
class ShaderPropertyDictionary : public BinaryDictionary<ShaderProperty, ShaderPropertyId>
{
public:
    ShaderPropertyDictionary() = default;
    virtual ~ShaderPropertyDictionary() override = default;

    static ShaderPropertyDictionary& GetInstance();
};

void InitShaderPropertyDictionary();

void WriteShaderPropertyDictionary(ByteWriter& stream);
void ReadShaderPropertyDictionary(ByteReader& stream);

ShaderPropertyId InternShaderProperty(const ShaderProperty& property);
bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty);

} // namespace Hyperion
