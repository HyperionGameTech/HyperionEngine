#include "Bindless.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {

/* The index of the descriptor we work on, /within/ the "bindless descriptor set" */
static const UInt bindless_descriptor_index = 0;

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

        for (const ID<Texture> &id : m_texture_ids) {
            descriptor->RemoveSubDescriptor(id.ToIndex());
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
            .element_index = texture->GetID().ToIndex(),
            .image_view = &texture->GetImageView(),
            .sampler = &texture->GetSampler()
        });
    }

    AssertThrow(indices[0] == indices[1] && indices[0] == (texture->GetID().ToIndex()));
    m_texture_ids.Insert(texture->GetID());
}

void BindlessStorage::RemoveResource(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->RemoveSubDescriptor(id.ToIndex());
    }

    m_texture_ids.Erase(id);
}

void BindlessStorage::MarkResourceChanged(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(id.ToIndex());
    }
}

bool BindlessStorage::GetResourceIndex(ID<Texture> id, UInt *out_index) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return false;
    }

    *out_index = id.ToIndex();

    return m_texture_ids.Contains(id);
}

} // namespace hyperion::v2
