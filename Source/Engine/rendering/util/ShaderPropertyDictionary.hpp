/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

namespace Hyperion {

/*! \brief The purpose of ShaderPropertyCache is to assign all ShaderProperty hashes to an
 *  index in a contiguous array upon first seeing it and then reusing that index for
 *  subsequent uses of the same ShaderProperty. This allows us to use bitsets to represent
 *  sets of ShaderProperties efficiently, rather than using HashSets (which are used for serializing and deserializing them). */

enum class ShaderPropertyId : uint32;

struct ShaderProperty;

class ByteWriter;

class BufferedReader;
using BufferedByteReader = BufferedReader;

void InitShaderPropertyDictionary();

void WriteShaderPropertyDictionary(ByteWriter& stream);
void ReadShaderPropertyDictionary(BufferedByteReader& stream);

ShaderPropertyId InternShaderProperty(const ShaderProperty& property);
bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty);

} // namespace Hyperion
