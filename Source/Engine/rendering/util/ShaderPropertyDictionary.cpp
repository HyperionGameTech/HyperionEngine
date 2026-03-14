/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

namespace Hyperion {

ShaderPropertyDictionary& ShaderPropertyDictionary::GetInstance()
{
    static ShaderPropertyDictionary s_instance;
    return s_instance;
}

void InitShaderPropertyDictionary()
{
    ShaderPropertyDictionary::GetInstance().Initialize();
}

void WriteShaderPropertyDictionary(ByteWriter& stream)
{
    ShaderPropertyDictionary::GetInstance().Write(stream);
}

void ReadShaderPropertyDictionary(ByteReader& stream)
{
    ShaderPropertyDictionary::GetInstance().Read(stream);
}

ShaderPropertyId InternShaderProperty(const ShaderProperty& property)
{
    return ShaderPropertyDictionary::GetInstance().Intern(property);
}

bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty)
{
    return ShaderPropertyDictionary::GetInstance().GetById(propertyId, outProperty);
}

} // namespace Hyperion
