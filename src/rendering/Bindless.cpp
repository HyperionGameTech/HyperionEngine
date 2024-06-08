/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Bindless.hpp>

#include <Engine.hpp>

namespace hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
}

void BindlessStorage::Destroy()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (const auto &it : m_resources) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), it.first.ToIndex(), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    m_resources.Clear();
}

void BindlessStorage::AddResource(ID<Texture> id, const ImageViewRef &image_view)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (!id.IsValid()) {
        return;
    }

    auto it = m_resources.Find(id);

    if (it != m_resources.End()) {
        return;
    }

    m_resources.Insert({ id, image_view });

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
            ->SetElement(NAME("Textures"), id.ToIndex(), image_view);
    }
}

void BindlessStorage::RemoveResource(ID<Texture> id)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (!id.IsValid()) {
        return;
    }

    auto it = m_resources.Find(id);

    if (it == m_resources.End()) {
        return;
    }

    m_resources.Erase(it);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
            ->SetElement(NAME("Textures"), id.ToIndex(), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
    }
}

} // namespace hyperion
