#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <core/lib/FixedArray.hpp>

namespace hyperion::v2 {

static class TransformComponentInterface : public ComponentInterface<TransformComponent>
{
public:
    TransformComponentInterface()
        : ComponentInterface<TransformComponent>(Array<ComponentProperty> {
            ComponentProperty(
                HYP_NAME_UNSAFE(Translation),
                [](const void *component) -> Any
                {
                    return Any::Construct<Vec3f>(static_cast<const TransformComponent *>(component)->transform.GetTranslation());
                },
                [](void *component, Any &&value)
                {
                    static_cast<TransformComponent *>(component)->transform.SetTranslation(value.Get<Vec3f>());
                }
            ),
            ComponentProperty(
                HYP_NAME_UNSAFE(Rotation),
                [](const void *component) -> Any
                {
                    return Any::Construct<Quaternion>(static_cast<const TransformComponent *>(component)->transform.GetRotation());
                },
                [](void *component, Any &&value)
                {
                    static_cast<TransformComponent *>(component)->transform.SetRotation(value.Get<Quaternion>());
                }
            ),
            ComponentProperty(
                HYP_NAME_UNSAFE(Scale),
                [](const void *component) -> Any
                {
                    return Any::Construct<Vec3f>(static_cast<const TransformComponent *>(component)->transform.GetScale());
                },
                [](void *component, Any &&value)
                {
                    static_cast<TransformComponent *>(component)->transform.SetScale(value.Get<Vec3f>());
                }
            ),
            ComponentProperty(
                HYP_NAME_UNSAFE(Matrix),
                [](const void *component) -> Any
                {
                    return Any::Construct<Matrix4>(static_cast<const TransformComponent *>(component)->transform.GetMatrix());
                }
            )
        })
    {
    }
} transform_component_interface;

} // namespace hyperion::v2