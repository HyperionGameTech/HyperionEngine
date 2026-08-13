/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/BinaryDictionary.hpp>

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

    static ENGINE_API ShaderPropertyDictionary& GetInstance();
};

void InitShaderPropertyDictionary();

void WriteShaderPropertyDictionary(ByteWriter& stream);
bool ReadShaderPropertyDictionary(ByteReader& stream);

ShaderPropertyId InternShaderProperty(const ShaderProperty& property);
bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty);

// ====================

inline void InitShaderPropertyDictionary()
{
    ShaderPropertyDictionary::GetInstance().Initialize();
}

inline void WriteShaderPropertyDictionary(ByteWriter& stream)
{
    ShaderPropertyDictionary::GetInstance().Write(stream);
}

inline bool ReadShaderPropertyDictionary(ByteReader& stream)
{
    return ShaderPropertyDictionary::GetInstance().Read(stream);
}

inline ShaderPropertyId InternShaderProperty(const ShaderProperty& property)
{
    return ShaderPropertyDictionary::GetInstance().Intern(property);
}

inline bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty)
{
    return ShaderPropertyDictionary::GetInstance().GetById(propertyId, outProperty);
}

} // namespace Hyperion
