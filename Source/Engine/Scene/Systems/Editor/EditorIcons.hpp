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

    // icons below are only shown in the editor UI
    AssetMesh,
    AssetTexture,
    AssetMaterial,
    AssetInstancedMesh,
    AssetAnimation,
    AssetAnimationTrack,
    AssetSkeleton,
    AssetWorld,
    AssetScene,
    AssetShader,
    AssetShaderBundle,
    AssetFontAtlas,
    AssetPhysicsShape,
    AssetScript,
    AssetRawData,
    AssetPrefab,
    AssetSound,
    AssetTerrain,
    AssetWeapon,

    MeshEditMode,

    Max
};

static constexpr uint32 NumIcons = uint32(Icon::Max);

/// Icons below this also get a sprite texture baked for the viewport
static constexpr uint32 NumSpriteIcons = uint32(Icon::AssetMesh);

static constexpr uint32 SpriteTextureSize = 256;
static constexpr uint32 UIImageSize = 64;

ENGINE_API SdfCanvas BuildCanvas(Icon icon);

ENGINE_API const SdfRasterStyle& GetSpriteRasterStyle();

ENGINE_API const char* GetFileStem(Icon icon);

ENGINE_API Name GetSpriteTextureName(Icon icon);

ENGINE_API Handle<Texture> CreateSpriteTexture(Icon icon, uint32 spriteTextureSize);
} // namespace EditorIcons

} // namespace Hyperion
