/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Reflection/BoxedValue.hpp>

namespace Hyperion
{

/*! \brief A single property override: the name of a reflected property on the Entity's class
 *  and its overridden value.
 *  Values are stored type-erased in memory, but are always serialized and parsed against the
 *  declared type of the property they override (see the $LayerOverrides schema section in HMF). */
struct LayerPropertyOverride
{
    Name property;
    BoxedValue value;
};

/*! \brief All property overrides for a single World Layer, applied on top of the Entity's base
 *  property set whenever that Layer becomes the World's active Layer.
 *  An Entity does not need to be a member of the Layer to carry an override set for it
 *  (membership controls visibility, overrides control property values). */
struct EntityLayerOverrideSet
{
    Name layerName;
    Array<LayerPropertyOverride> propertyOverrides;
};

} // namespace Hyperion
