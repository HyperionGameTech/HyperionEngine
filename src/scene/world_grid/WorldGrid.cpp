/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <WorldGrid.generated.inl>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(WorldGrid, Scene);

#pragma region WorldGridState

void WorldGridState::PushUpdate(StreamingCellUpdate&& update)
{
    Mutex::Guard guard(patchUpdateQueueMutex);

    patchUpdateQueue.Push(std::move(update));

    patchUpdateQueueSize.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
}

#pragma endregion WorldGridState

#pragma region WorldGrid

WorldGrid::WorldGrid()
    : WorldGrid(nullptr)
{
}

WorldGrid::WorldGrid(World* world)
    : m_world(world)
{
}

WorldGrid::~WorldGrid()
{
    if (IsReady())
    {
        Shutdown();
    }
}

void WorldGrid::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    ObjectBase::Init();

    // Add a default layer if none are provided
    if (m_layers.Empty())
    {
        HYP_LOG(WorldGrid, Info, "No layers provided to WorldGrid, creating default layer");

        m_layers.PushBack(CreateObject<WorldGridLayer>());
    }

    for (const Handle<WorldGridLayer>& layer : m_layers)
    {
        InitObject(layer);

        layer->OnAdded(this);

        g_streamingManager->AddWorldGridLayer(layer);
    }

    SetReady(true);
}

void WorldGrid::Shutdown()
{
    if (!IsReady())
    {
        return;
    }

    for (const Handle<WorldGridLayer>& layer : m_layers)
    {
        if (!layer.IsValid())
        {
            continue;
        }

        layer->OnRemoved(this);
    }

    g_streamingManager->Stop();

    SetReady(false);
}

void WorldGrid::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    AssertReady();

    g_streamingManager->Update(delta);
}

void WorldGrid::AddLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!layer.IsValid())
    {
        return;
    }

    if (m_layers.Contains(layer))
    {
        return;
    }

    m_layers.PushBack(layer);

    if (IsReady())
    {
        InitObject(layer);

        layer->OnAdded(this);

        g_streamingManager->AddWorldGridLayer(layer);
    }
}

bool WorldGrid::RemoveLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!layer)
    {
        return false;
    }

    auto it = m_layers.FindAs(layer);

    if (it != m_layers.End())
    {
        if (IsReady())
        {
            (*it)->OnRemoved(this);

            g_streamingManager->RemoveWorldGridLayer(*it);
        }

        m_layers.Erase(it);

        return true;
    }

    return false;
}
;
void WorldGrid::SetStreamingLayersFromDescs(Span<const WGLayerDesc> descs)
{
    HYP_SCOPE;

    const bool isReady = IsReady();

    if (isReady)
    {
        AssertOnThread(g_gameThread);
    }

    if (isReady)
    {
        for (auto& layer : m_layers)
        {
            layer->OnRemoved(this);

            g_streamingManager->RemoveWorldGridLayer(layer);
        }
    }

    m_layers.Clear();

    for (const WGLayerDesc& layerDesc : descs)
    {
        const Class* cls = GetClass(layerDesc.className);

        if (!cls)
        {
            HYP_LOG(WorldGrid, Error, "Attempted to add layer of class '{}' but the class was not found!", layerDesc.className);

            continue;
        }

        if (!cls->IsDerivedFrom(WorldGridLayer::StaticClass()))
        {
            HYP_LOG(WorldGrid, Error, "Attempted to add layer of class '{}' but it is not derived from WorldGridLayer!", cls->GetName());

            continue;
        }
        
        HypData instance;
        if (!cls->CreateInstance(instance, /* allowAbstract */ false))
        {
            HYP_LOG(WorldGrid, Error, "Failed to create instance of layer class '{}'!", cls->GetName());

            continue;
        }

        AssertDebug(instance.Is<Handle<WorldGridLayer>>());

        Handle<WorldGridLayer>& layer = instance.Get<Handle<WorldGridLayer>>();
        AssertDebug(layer != nullptr);

        // setup layer members

        layer->m_layerInfo = layerDesc.info;

        for (const WGObject& object : layerDesc.objects)
        {
            layer->m_objectsByCoord[object.coords].PushBack(AssetReference(object.path));
        }

        if (isReady)
        {
            layer->OnAdded(this);

            g_streamingManager->AddWorldGridLayer(layer);
        }

        m_layers.PushBack(std::move(layer));
    }
}

Array<WGLayerDesc> WorldGrid::GetStreamingLayerDescs() const
{
    Array<WGLayerDesc> descs;
    descs.Reserve(m_layers.Size());

    for (const Handle<WorldGridLayer>& layer : m_layers)
    {
        WGLayerDesc& layerDesc = descs.EmplaceBack();
        layerDesc.className = layer->InstanceClass()->GetName();
        layerDesc.info = layer->GetLayerInfo();

        for (const KeyValuePair<Vec2i, Array<WGObject>>& pair : layer->m_objectsByCoord)
        {
            layerDesc.objects.Concat(pair.second);
        }
    }

    return descs;
}

#pragma endregion WorldGrid

} // namespace hyperion
