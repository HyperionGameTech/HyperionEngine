#include "audio_control.h"
#include "../entity.h"

namespace hyperion {
AudioControl::AudioControl(std::shared_ptr<AudioSource> source)
    : EntityControl(fbom::FBOMObjectType("AUDIO_CONTROL"), 10),
      m_source(source)
{
}

void AudioControl::OnAdded()
{
    if (m_source != nullptr) {
        m_source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnRemoved()
{
    if (m_source != nullptr) {
        m_source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnUpdate(double dt)
{
    auto &current_position = parent->GetGlobalTransform().GetTranslation();
    if (m_source != nullptr && m_last_position != current_position) {
        m_source->SetPosition(current_position);
        m_last_position = current_position;
    }
}

std::shared_ptr<EntityControl> AudioControl::CloneImpl()
{
    auto clone = std::make_shared<AudioControl>(m_source); // should source be cloned?

    return clone;
}
} // namespace hyperion
