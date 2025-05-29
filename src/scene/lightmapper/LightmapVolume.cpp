/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/Texture.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/Threads.hpp>

#include <Engine.hpp>

namespace hyperion {

LightmapVolume::LightmapVolume()
    : m_render_resource(nullptr),
      m_aabb(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_render_resource(nullptr),
      m_aabb(aabb)
{
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);

        if (m_render_resource != nullptr)
        {
            FreeResource(m_render_resource);
        }
    }
}

bool LightmapVolume::AddElement(LightmapElement element)
{
    Threads::AssertOnThread(g_game_thread);

    if (IsInitCalled())
    {
        for (LightmapElementTextureEntry& entry : element.entries)
        {
            InitObject(entry.texture);
        }
    }

    if (element.index == ~0u)
    {
        element.index = uint32(m_elements.Size());

        m_elements.PushBack(std::move(element));

        return true;
    }

    const uint32 index = element.index;

    if (index < m_elements.Size())
    {
        if (m_elements[index].IsValid())
        {
            return false; // cannot overwrite existing element at that index.
        }
    }
    else
    {
        m_elements.Resize(index + 1);
    }

    Swap(m_elements[index], element);

    return true;
}

const LightmapElement* LightmapVolume::GetElement(uint32 index) const
{
    Threads::AssertOnThread(g_game_thread);

    if (index >= m_elements.Size())
    {
        return nullptr;
    }

    return &m_elements[index];
}

void LightmapVolume::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind(
        [this]()
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderLightmapVolume>(this);

    for (LightmapElement& element : m_elements)
    {
        if (!element.IsValid())
        {
            continue;
        }

        for (LightmapElementTextureEntry& entry : element.entries)
        {
            InitObject(entry.texture);
        }
    }

    SetReady(true);
}

} // namespace hyperion
