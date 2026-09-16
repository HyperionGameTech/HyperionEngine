/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Asset/AssetBucket.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Shader.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

using namespace Hyperion;

enum class ShaderPropertyOptionKind : uint8
{
    Toggle = 0, // a PERMUTE(NAME) - either present or absent
    Value = 1   // one member of a PERMUTE(NAME, ...) value group - at most one may be present
};

// Mirrored by ShaderProperties.ShaderPropertyOptionData on the managed side.
struct ShaderPropertyOptionBinding
{
    Name name;
    char valueString[64];
    uint32 propertyId;
    uint8 kind;
};

static_assert(sizeof(ShaderPropertyOptionBinding) == 80, "Managed ShaderPropertyOptionData must match this layout");

static void WriteOption(ShaderPropertyOptionBinding& out, const ShaderProperty& property, ShaderPropertyId propertyId)
{
    Memory::Zero(&out, sizeof(out));

    out.name = property.name;
    out.propertyId = uint32(propertyId);

    const String valueString = property.GetValueString();

    if (valueString.Any())
    {
        out.kind = uint8(ShaderPropertyOptionKind::Value);

        // Truncates and null-terminates on its own; the zeroed buffer covers the empty case.
        Memory::CopyString(out.valueString, valueString.Data(), Memory::StrLen(valueString.Data()));
    }
    else
    {
        out.kind = uint8(ShaderPropertyOptionKind::Toggle);
    }
}

extern "C"
{
    /*! \brief Collect the shader properties a variant of \p pShaderName can be built with.
     *  Every property a compiled variant carries that isn't one of the bundle's static properties
     *  came from a permutation, so the union over the bundle's variants is exactly the set of
     *  options an editor can offer. Pass a null \p pOut to query the count. */
    HYP_EXPORT uint32 ShaderProperties_GetShaderOptions(const Name* pShaderName, ShaderPropertyOptionBinding* pOut, uint32 maxCount)
    {
        if (!pShaderName || !pShaderName->IsValid())
        {
            return 0;
        }

        Handle<ShaderBundle> bundle = GetEngineAssetRegistry()->GetAsset<ShaderBundle>(AssetBuckets::ShaderBundles, *pShaderName);

        if (!bundle.IsValid())
        {
            return 0;
        }

        Array<ShaderPropertyId> optionIds;

        for (const Handle<Shader>& shader : bundle->compiledShaders)
        {
            if (!shader.IsValid())
            {
                continue;
            }

            for (ShaderPropertyId propertyId : shader->properties.ToArray())
            {
                if (bundle->staticProperties.Contains(propertyId) || optionIds.Contains(propertyId))
                {
                    continue;
                }

                optionIds.PushBack(propertyId);
            }
        }

        if (!pOut)
        {
            return uint32(optionIds.Size());
        }

        if (maxCount > uint32(optionIds.Size()))
        {
            maxCount = uint32(optionIds.Size());
        }

        uint32 numWritten = 0;

        for (uint32 i = 0; i < maxCount; i++)
        {
            ShaderProperty property;

            if (!GetShaderPropertyById(optionIds[i], property))
            {
                continue;
            }

            WriteOption(pOut[numWritten++], property, optionIds[i]);
        }

        return numWritten;
    }

    /*! \brief Describe a single interned shader property, for set bits that aren't among the
     *  shader's own options (a property left over from another shader, say). */
    HYP_EXPORT int8 ShaderProperties_GetPropertyById(uint32 propertyId, ShaderPropertyOptionBinding* pOut)
    {
        if (!pOut)
        {
            return 0;
        }

        ShaderProperty property;

        if (!GetShaderPropertyById(ShaderPropertyId(propertyId), property))
        {
            return 0;
        }

        WriteOption(*pOut, property, ShaderPropertyId(propertyId));

        return 1;
    }
} // extern "C"
