#include "bindless.h"

#include <engine.h>

namespace hyperion::v2 {

BindlessStorage::BindlessStorage()
    : m_descriptor_sets{}
{
}

BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create(Engine *engine)
{
    m_descriptor_sets[0] = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS);
    m_descriptor_sets[1] = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1);
}

void BindlessStorage::Destroy(Engine *engine)
{
    /* Remove all texture sub-descriptors */
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_ids) {
            descriptor->RemoveSubDescriptor(it.value - 1);
        }
    }

    m_texture_ids.Clear();
}

void BindlessStorage::ApplyUpdates(Engine *engine, uint32_t frame_index)
{
    auto *descriptor_set = m_descriptor_sets[frame_index % m_descriptor_sets.size()];

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void BindlessStorage::AddResource(const Texture *texture)
{
    AssertThrow(texture != nullptr);
    
    uint32_t indices[2] = {};

    for (size_t i = 0; i < m_descriptor_sets.size(); i++) {
        auto *descriptor_set = m_descriptor_sets[i];
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);
        
        indices[i] = descriptor->AddSubDescriptor({
            .element_index = texture->GetId().value - 1,
            .image_view    = &texture->GetImageView(),
            .sampler       = &texture->GetSampler()
        });
    }

    AssertThrow(indices[0] == indices[1] && indices[0] == (texture->GetId().value - 1));
    m_texture_ids.Insert(texture->GetId());
}

void BindlessStorage::RemoveResource(const Texture *texture)
{
    if (texture == nullptr) {
        return;
    }

    const auto id = texture->GetId();

    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->RemoveSubDescriptor(id.value - 1);
    }

    m_texture_ids.Erase(id);
}

void BindlessStorage::MarkResourceChanged(const Texture *texture)
{
    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(texture->GetId().value - 1);
    }
}

bool BindlessStorage::GetResourceIndex(const Texture *texture, uint32_t *out_index) const
{
    if (texture == nullptr) {
        return false;
    }

    const auto id = texture->GetId();

    *out_index = id.value - 1;

    return m_texture_ids.Contains(id);
}

} // namespace hyperion::v2
