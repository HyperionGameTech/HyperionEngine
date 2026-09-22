/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/Util/SdfCanvas.hpp>

namespace Hyperion {

class Texture;

namespace EditorIcons {

enum class Icon : uint32
{
    PointLight = 0,
    SpotLight,
    DirectionalLight,
    AreaLight,
    Camera,
    EnvProbe,
    LightmapVolume,
    Node,
    Entity,
    Sprite,
    TextSprite,
    ParticleVolume,
    FogVolume,
    Decal,

    Max
};

static constexpr uint32 NumIcons = uint32(Icon::Max);

static constexpr uint32 SpriteTextureSize = 256;
static constexpr uint32 UIImageSize = 64;

ENGINE_API SdfCanvas BuildCanvas(Icon icon);

ENGINE_API const SdfRasterStyle& GetSpriteRasterStyle();

ENGINE_API const char* GetFileStem(Icon icon);

ENGINE_API Name GetSpriteTextureName(Icon icon);

ENGINE_API Handle<Texture> CreateSpriteTexture(Icon icon, uint32 spriteTextureSize);
} // namespace EditorIcons

} // namespace Hyperion
