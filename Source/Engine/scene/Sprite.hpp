/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/math/Color.hpp>

#include <scene/Entity.hpp>

namespace Hyperion {

class Texture;
class Scene;
class EnvProbe;
class LightmapVolume;
class Camera;

HYP_ENUM()
enum class SpriteType : uint32
{
    None = 0,

    Point2D,
    Spot3D,
    Crosshair,

    EnvProbe,
    LightmapVolume,
    Camera,

    Max
};

HYP_CLASS(AssetBucket = "Sprites")
class HYP_API Sprite : public Entity
{
    HYP_OBJECT_BODY(Sprite);

public:
    Sprite();
    explicit Sprite(SpriteType spriteType);

    Sprite(const Sprite& other) = delete;
    Sprite& operator=(const Sprite& other) = delete;

    ~Sprite() override;

    void UpdateRenderProxy(class RenderProxySprite* proxy);

    static Handle<Sprite> CreateEnvProbeSprite(Scene* scene, EnvProbe* envProbe);
    static Handle<Sprite> CreateLightmapVolumeSprite(Scene* scene, LightmapVolume* lightmapVolume);
    static Handle<Sprite> CreateCameraSprite(Scene* scene, Camera* camera);

    SpriteType spriteType = SpriteType::None;
    float size = 1.0f;
    Color color = Color::White();
    Handle<Texture> texture;
    float opacity = 1.0f;
    bool alwaysFaceCamera = true;

    Handle<EnvProbe> m_envProbe;
    Handle<LightmapVolume> m_lightmapVolume;
    Handle<Camera> m_camera;

protected:
    void Init() override;
};

} // namespace Hyperion