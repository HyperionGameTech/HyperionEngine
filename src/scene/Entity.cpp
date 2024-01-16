#include <scene/Entity.hpp>
#include <math/BoundingSphere.hpp>
#include <rendering/RenderGroup.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

namespace hyperion::v2 {

Entity::Entity()
    : BasicObject()
{
}

Entity::Entity(Name name)
    : BasicObject(name)
{
}

void Entity::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    SetReady(true);
}

Bool Entity::IsReady() const
{
    return Base::IsReady();
}

} // namespace hyperion::v2
