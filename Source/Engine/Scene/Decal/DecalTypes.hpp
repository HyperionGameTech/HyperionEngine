/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Util.hpp>

namespace Hyperion {

enum class DecalId : uint32;
static constexpr DecalId InvalidDecalId = Invalid<DecalId>;

class Material;

HYP_STRUCT()
struct DecalDesc
{
    HYP_STRUCT_BODY(DecalDesc);

    HYP_FIELD(Property = "Material", Serialize, Editor, Title = "Decal Material", Description = "The material used to draw the decal")
    Handle<Material> material;

    HYP_FIELD(Property = "DefaultSize", Serialize, Editor, Title = "Default Size", Description = "Size of the projection box in world units")
    Vec3f defaultSize = Vec3f(1.0f, 0.5f, 1.0f);

    HYP_FIELD(Property = "Opacity", Serialize, Editor, Title = "Opacity")
    float opacity = 1.0f;

    HYP_FIELD(Property = "NormalStrength", Serialize, Editor, Title = "Normal Strength")
    float normalStrength = 1.0f;

    HYP_FIELD(Property = "AngleFadeStart", Serialize, Editor, Title = "Angle Fade Start", Description = "Cosine of the angle between the surface normal and the projection axis at which the decal starts to fade out")
    float angleFadeStart = 0.5f;

    HYP_FIELD(Property = "AngleFadeEnd", Serialize, Editor, Title = "Angle Fade End", Description = "Cosine of the angle at which the decal is fully faded out")
    float angleFadeEnd = 0.2f;

    HYP_FIELD(Property = "ExcludeMask", Serialize, Editor, Title = "Exclude Mask", Description = "Object mask bits of surfaces the decal skips (unlit = 1, lightmapped = 2, foliage = 4, alpha cutout = 8)")
    uint8 excludeMask = 0;

    HYP_FIELD(Property = "SortOrder", Serialize, Editor, Title = "Sort Order", Description = "Decals with a higher sort order are drawn on top")
    int32 sortOrder = 0;
};

HYP_STRUCT()
struct DecalInstance
{
    HYP_STRUCT_BODY(DecalInstance);

    HYP_FIELD(Property = "Id", Serialize)
    DecalId id = InvalidDecalId;

    HYP_FIELD(Property = "Transform", Serialize)
    Transform transform;
};

} // namespace Hyperion
