/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Util.hpp>

#ifdef HYP_EDITOR
#include <Baking/BakeLayer.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

enum class LayerId : uint32;
static constexpr LayerId InvalidLayerId = Invalid<LayerId>;

HYP_CLASS()
class ENGINE_API Layer final : public ObjectBase
{
    HYP_OBJECT_BODY(Layer);

public:
    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "LayerId", Serialize)
    LayerId layerId = InvalidLayerId;

#ifdef HYP_EDITOR
    HYP_FIELD(Property = "BakeLayer", EditorOnly, Serialize)
    Baking::BakeLayer bakeLayer;
#endif // HYP_EDITOR

    Layer() = default;

    Layer(Name name, LayerId layerId)
        : name(name),
          layerId(layerId)
#ifdef HYP_EDITOR
         , bakeLayer(name)
#endif // HYP_EDITOR
    {
    }
};

} // namespace Hyperion
