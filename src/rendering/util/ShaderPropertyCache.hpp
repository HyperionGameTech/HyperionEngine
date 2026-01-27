/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

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

HYP_API void WriteShaderPropertyDatabase(ByteWriter& stream);
HYP_API void ReadShaderPropertyDatabase(BufferedByteReader& stream);

HYP_API HYP_NODISCARD ShaderPropertyId InternShaderProperty(const ShaderProperty& property);

} // namespace Hyperion
