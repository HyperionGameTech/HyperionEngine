/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/ObjectPool.hpp>

// Debugging
#include <core/containers/HashMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/debug/StackDump.hpp>

// temp
#include <scene/Entity.hpp>

namespace hyperion {

// debugging
static HashMap<uint32, Pair<Array<String>, Array<String>>> g_entity_stack_traces { };
static Mutex g_entity_stack_traces_mutex { };

// Debugging
void TraceLiveEntities()
{
    auto *holder = dynamic_cast<ObjectContainer<Entity> *>(ObjectPool::GetObjectContainerHolder().TryGet(TypeID::ForType<Entity>()));
    if (!holder) {
        return;
    }

    uint32 num = holder->m_pool.NumAllocatedElements();

    Mutex::Guard guard(g_entity_stack_traces_mutex);

    for (uint32 i = 0; i < num; i++) {
        auto &element = holder->m_pool.GetElement(i);
        DebugLog(LogType::Debug, "ID: %u\tStrong: %u\tWeak: %u\n",
            element.index + 1,
            element.GetRefCountStrong(),
            element.GetRefCountWeak());

        if (element.GetRefCountStrong() != 0) {
            auto &traces = g_entity_stack_traces[element.index];

            DebugLog(LogType::Debug, "\tIncRefStrong traces:\n");

            if (traces.first.Any()) {
                for (auto &trace : traces.first) {
                    DebugLog(LogType::Debug, "\t\t%s\n", trace.Data());
                }
            } else {
                DebugLog(LogType::Debug, "\t\t--NONE--\n");
            }


            DebugLog(LogType::Debug, "\tDecRefStrong traces:\n");

            if (traces.second.Any()) {
                for (auto &trace : traces.second) {
                    DebugLog(LogType::Debug, "\t\t%s\n", trace.Data());
                }
            } else {
                DebugLog(LogType::Debug, "\t\t--NONE--\n");
            }
        }
    }
}

void TraceIncEntityRef(uint32 index)
{
    Mutex::Guard guard(g_entity_stack_traces_mutex);

    g_entity_stack_traces[index].first.PushBack(StackDump(3, 6).GetTrace().Front());
}

void TraceDecEntityRef(uint32 index)
{
    Mutex::Guard guard(g_entity_stack_traces_mutex);

    g_entity_stack_traces[index].second.PushBack(StackDump(3, 6).GetTrace().Front());
}

void TraceRemoveEntityRef(uint32 index)
{
    Mutex::Guard guard(g_entity_stack_traces_mutex);

    auto it = g_entity_stack_traces.Find(index);

    if (it != g_entity_stack_traces.End()) {
        g_entity_stack_traces.Erase(it);
    }
}

ObjectPool::ObjectContainerMap &ObjectPool::GetObjectContainerHolder()
{
    static ObjectPool::ObjectContainerMap holder { };

    return holder;
}

IObjectContainer &ObjectPool::ObjectContainerMap::Add(TypeID type_id)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    // Already allocated
    if (it != m_map.End()) {
        return *it->second;
    }

    m_map.PushBack({
        type_id,
        nullptr
    });

    return *m_map.Back().second;
}

IObjectContainer &ObjectPool::ObjectContainerMap::Get(TypeID type_id)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    if (it == m_map.End()) {
        HYP_FAIL("No object container for TypeID: %u", type_id.Value());
    }

    AssertThrow(it->second != nullptr);

    return *it->second;
}

IObjectContainer *ObjectPool::ObjectContainerMap::TryGet(TypeID type_id)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([type_id](const auto &element)
    {
        return element.first == type_id;
    });

    if (it == m_map.End()) {
        return nullptr;
    }

    return it->second.Get();
}

} // namespace hyperion