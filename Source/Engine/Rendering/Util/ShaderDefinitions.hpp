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

#include <Core/Utilities/EnumFlags.hpp>

namespace Hyperion {

HYP_ENUM()
enum class ShaderCookKind : uint8
{
    Pass = 0,
    Material,
    EditorOnly
};

HYP_ENUM()
enum class ShaderTraits : uint32
{
    None = 0x0,
    GeometryPass = 0x1,
    ForwardClustered = 0x2,
    Terrain = 0x4,
    Morph = 0x8,
    Foliage = 0x10
};

HYP_MAKE_ENUM_FLAGS(ShaderTraits);

HYP_STRUCT()
struct ShaderDefinition
{
    HYP_STRUCT_BODY(ShaderDefinition);

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "Cook", Serialize)
    ShaderCookKind cookKind = ShaderCookKind::Pass;

    HYP_FIELD(Property = "Traits", Serialize)
    EnumFlags<ShaderTraits> traits = ShaderTraits::None;

    HYP_FIELD(Property = "Condition", Serialize)
    String condition; // a cvar to check

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

ENGINE_API const ShaderDefinitions* GetShaderDefinitions();

} // namespace Hyperion
