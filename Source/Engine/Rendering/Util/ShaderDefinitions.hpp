/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

HYP_STRUCT()
struct ShaderDefinition
{
    HYP_STRUCT_BODY(ShaderDefinition);

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "VS", Serialize)
    String vertexShader;

    HYP_FIELD(Property = "PS", Serialize)
    String pixelShader;

    HYP_FIELD(Property = "CS", Serialize)
    String computeShader;

    HYP_FIELD(Property = "RGS", Serialize)
    String rayGenShader;

    HYP_FIELD(Property = "CHS", Serialize)
    String closestHitShader;

    HYP_FIELD(Property = "MS", Serialize)
    String missShader;
};

HYP_STRUCT()
struct ShaderDefinitions
{
    HYP_STRUCT_BODY(ShaderDefinitions);

    HYP_FIELD(Property = "Definitions", Serialize)
    Array<ShaderDefinition> definitions;

    HYP_FORCE_INLINE bool HasShader(StringHash nameHash) const
    {
        auto it = definitions.FindIf([nameHash](const ShaderDefinition &definition)
        {
            return definition.name == nameHash;
        });

        return it != definitions.End();
    }

    HYP_FORCE_INLINE const ShaderDefinition* FindEntry(StringHash nameHash) const
    {
        auto it = definitions.FindIf([nameHash](const ShaderDefinition &definition)
        {
            return definition.name == nameHash;
        });

        if (it == definitions.End())
        {
            return nullptr;
        }

        return it;
    }
};

} // namespace Hyperion
