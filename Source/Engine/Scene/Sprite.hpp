/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/Math/Color.hpp>

#include <Scene/Entity.hpp>

namespace Hyperion {

class Texture;

HYP_ENUM()
enum class SpriteType : uint32
{
    None = 0,

    Text,
    
    ///Editor
    Editor_EnvProbe,
    Editor_LightmapVolume,
    Editor_Camera,
    Editor_Light,
    ////////////////////

    Max
};

HYP_CLASS()
class ENGINE_API Sprite : public Entity
{
    HYP_OBJECT_BODY(Sprite);

public:
    Sprite();
    Sprite(Name name, SpriteType spriteType);

    Sprite(const Sprite& other) = delete;
    Sprite& operator=(const Sprite& other) = delete;

    ~Sprite() override;

    virtual void UpdateRenderProxy(struct RenderProxySprite* proxy);

    HYP_FIELD(Property = "SpriteType", Serialize = false, EditEnabled = false)
    SpriteType spriteType = SpriteType::None;
    
    HYP_FIELD(Property = "Size", Serialize, Editor)
    float size = 1.0f;
    
    HYP_FIELD(Property = "Color", Serialize, Editor)
    Color color = Color::White();
    
    HYP_FIELD(Property = "Opacity", Serialize, Editor)
    float opacity = 1.0f;
    
    HYP_FIELD(Property = "AlwaysFaceCamera", Serialize, Editor)
    bool alwaysFaceCamera = true;
    
    Handle<Texture> texture;
};

} // namespace Hyperion
