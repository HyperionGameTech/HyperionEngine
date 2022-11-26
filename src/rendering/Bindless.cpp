#include "Bindless.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {

BindlessStorage::BindlessStorage()
    : m_descriptor_sets { }
{
}

BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_descriptor_sets[0] = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS);
    m_descriptor_sets[1] = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1);
}

void BindlessStorage::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    /* Remove all texture sub-descriptors */
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_ids) {
            descriptor->RemoveSubDescriptor(it.value - 1);
        }
    }

    m_texture_ids.Clear();
}

void BindlessStorage::AddResource(const Texture *texture)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(texture != nullptr);
    
    UInt indices[] = { 0, 0 };

    for (SizeType i = 0; i < m_descriptor_sets.Size(); i++) {
        auto *descriptor_set = m_descriptor_sets[i];
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);
        
        indices[i] = descriptor->SetSubDescriptor({
            .element_index = texture->GetID().value - 1,
            .image_view = &texture->GetImageView(),
            .sampler = &texture->GetSampler()
        });
    }

    AssertThrow(indices[0] == indices[1] && indices[0] == (texture->GetID().value - 1));
    m_texture_ids.Insert(texture->GetID());
}

void BindlessStorage::RemoveResource(IDBase id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (id == Texture::empty_id) {
        return;
    }

    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->RemoveSubDescriptor(id.value - 1);
    }

    m_texture_ids.Erase(id);
}

void BindlessStorage::MarkResourceChanged(const Texture *texture)
{
    Threads::AssertOnThread(THREAD_RENDER);

    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(texture->GetID().value - 1);
    }
}

bool BindlessStorage::GetResourceIndex(const Texture *texture, uint32_t *out_index) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (texture == nullptr) {
        return false;
    }

    const auto id = texture->GetID();

    *out_index = id.value - 1;

    return m_texture_ids.Contains(id);
}

} // namespace hyperion::v2
