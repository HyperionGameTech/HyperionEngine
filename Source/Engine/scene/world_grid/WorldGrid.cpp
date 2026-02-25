/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>

#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <streaming/StreamingManager.hpp>

#include <Core/threading/TaskSystem.hpp>

#include <Core/utilities/ForEach.hpp>

#include <engine/EngineDriver.hpp>

#include <WorldGrid.generated.inl>

namespace Hyperion {

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
    AssertOnThread(g_simThread);

    ObjectBase::Init();

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
}

void WorldGrid::AddLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

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
    AssertOnThread(g_simThread);

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

void WorldGrid::SetStreamingLayersFromDescs(Span<const WGLayerDesc> descs)
{
    HYP_SCOPE;

    const bool isReady = IsReady();

    if (isReady)
    {
        AssertOnThread(g_simThread);

        for (Handle<WorldGridLayer>& layer : m_layers)
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

        BoxedValue instance;
        if (!cls->CreateInstance(instance, /* allowAbstract */ false))
        {
            HYP_LOG(WorldGrid, Error, "Failed to create instance of layer class '{}'!", cls->GetName());

            continue;
        }

        AssertDebug(instance.Is<Handle<WorldGridLayer>>());

        Handle<WorldGridLayer>& layer = instance.Get<Handle<WorldGridLayer>>();
        AssertDebug(layer != nullptr);

        layer->SetName(layerDesc.layerName);

        // setup layer members

        layer->m_layerInfo = layerDesc.info;

        for (const WGObject& object : layerDesc.objects)
        {
            layer->m_objectsByCoord[object.coords].PushBack(AssetReference(object.path));
        }

        if (isReady)
        {
            InitObject(layer);

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
        layerDesc.layerName = layer->GetName();
        layerDesc.info = layer->GetLayerInfo();

        for (const KeyValuePair<Vec2i, Array<AssetReference, DynamicAllocator>>& pair : layer->m_objectsByCoord)
        {
            const Vec2i& coord = pair.first;
            const Array<AssetReference, DynamicAllocator>& assetReferences = pair.second;

            layerDesc.objects.Reserve(layerDesc.objects.Size() + assetReferences.Size());

            for (const AssetReference& assetReference : assetReferences)
            {
                layerDesc.objects.PushBack(WGObject { coord, assetReference.GetAssetPath() });
            }
        }
    }

    return descs;
}

#pragma endregion WorldGrid

} // namespace Hyperion
