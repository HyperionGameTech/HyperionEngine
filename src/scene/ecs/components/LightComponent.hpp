#ifndef HYPERION_V2_ECS_LIGHT_COMPONENT_HPP
#define HYPERION_V2_ECS_LIGHT_COMPONENT_HPP

#include <core/Handle.hpp>
#include <rendering/Light.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

using LightComponentFlags = uint32;

enum LightComponentFlagBits : LightComponentFlags
{
    LIGHT_COMPONENT_FLAGS_NONE  = 0x00,
    LIGHT_COMPONENT_FLAGS_INIT  = 0x01
};

struct LightComponent
{
    Handle<Light>           light;
    HashCode                transform_hash_code;
    LightComponentFlags     flags = LIGHT_COMPONENT_FLAGS_NONE;
};

} // namespace hyperion::v2

#endif