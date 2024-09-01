#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(AudioComponent)
    HYP_FIELD(audio_source),
    HYP_FIELD(playback_state),
    HYP_FIELD(flags),
    HYP_FIELD(last_position),
    HYP_FIELD(timer)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(AudioComponent);

} // namespace hyperion