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

    m_descriptor_sets[0] = g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS);
    m_descriptor_sets[1] = g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1);
}

void BindlessStorage::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    /* Remove all texture sub-descriptors */
    for (const DescriptorSetRef &descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_ids) {
            descriptor->RemoveSubDescriptor(it.first.ToIndex());
        }
    }

    m_texture_ids.Clear();
}

void BindlessStorage::AddResource(Texture *texture)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(texture != nullptr);

    for (SizeType i = 0; i < m_descriptor_sets.Size(); i++) {
        const DescriptorSetRef &descriptor_set = m_descriptor_sets[i];
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        // TODO: More generic factory
        Sampler *sampler = nullptr;

        switch (texture->GetFilterMode()) {
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            sampler = g_engine->GetPlaceholderData()->GetSamplerLinearMipmap();

            break;
        case FilterMode::TEXTURE_FILTER_LINEAR:
            sampler = g_engine->GetPlaceholderData()->GetSamplerLinear();

            break;
        case FilterMode::TEXTURE_FILTER_NEAREST: // fallthrough
        default:
            sampler = g_engine->GetPlaceholderData()->GetSamplerNearest();

            break;
        }
        
        descriptor->SetElementImageSamplerCombined(
            texture->GetID().ToIndex(),
            texture->GetImageView(),
            sampler
        );
    }
    
    const auto insert_result = m_texture_ids.Insert(texture->GetID(), texture);

    DebugLog(LogType::Debug, "Bindless: Add texture %u. Count: %llu\n", texture->GetID().Value(), m_texture_ids.Size());

    AssertThrowMsg(insert_result.second, "Duplicate AddResource call for index %u!", texture->GetID().ToIndex());
}

void BindlessStorage::RemoveResource(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (const DescriptorSetRef &descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->SetElementImageSamplerCombined(
            id.ToIndex(),
            g_engine->GetPlaceholderData()->GetImageView2D1x1R8(),
            g_engine->GetPlaceholderData()->GetSamplerLinear()
        );
    }

    m_texture_ids.Erase(id);
}

void BindlessStorage::MarkResourceChanged(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (const DescriptorSetRef &descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(id.ToIndex());
    }
}

Texture *BindlessStorage::GetResource(ID<Texture> id) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return nullptr;
    }

    const auto it = m_texture_ids.Find(id);

    if (it == m_texture_ids.End()) {
        return nullptr;
    }

    return it->second;
}

} // namespace hyperion::v2
