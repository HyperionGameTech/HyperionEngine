/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Sprite.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/Texture.hpp>

#include <Sprite.generated.inl>

namespace Hyperion {

Sprite::Sprite()
    : Sprite(Name::Invalid(), SpriteType::None)
{
}

Sprite::Sprite(Name name, SpriteType spriteType)
    : Entity(name),
      spriteType(spriteType)
{
    m_nodeFlags |= (NodeFlags::ExcludeFromParentBounds | NodeFlags::ExcludeFromOctree);
}

Sprite::~Sprite() = default;

void Sprite::UpdateRenderProxy(RenderProxySprite* proxy)
{
    proxy->sprite = this;

    if (proxy->texture != texture.Get())
    {
        proxy->texture = texture.Get();
        proxy->forceRebind = true;
    }

    SpriteShaderData& bufferData = proxy->bufferData;
    bufferData.positionSize = Vec4f(GetWorldTranslation(), size);
    bufferData.color = Vec4f(color);
    bufferData.spriteType = uint32(spriteType);
    bufferData.opacity = opacity;
    bufferData.alwaysFaceCamera = alwaysFaceCamera ? 1u : 0u;
    bufferData.textureIndex = ~0u;
}

} // namespace Hyperion
