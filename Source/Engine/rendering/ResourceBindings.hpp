#pragma once

#include <Core/Defines.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/Class.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <Core/threading/Thread.hpp>

namespace Hyperion {

#pragma region ResourceBindings

#include <rendering/ResourceBindings.inc>

struct SubtypeResourceBindings
{
    const Class* resourceClass;
    GpuBufferHolderBase* gpuBufferHolder;
    SparsePagedArray<uint32, 1024, RenderAllocator> bindingIndices;

    SubtypeResourceBindings(const Class* resourceClass, GpuBufferHolderBase* gpuBufferHolder)
        : resourceClass(resourceClass),
          gpuBufferHolder(gpuBufferHolder)
    {
        AssertDebug(resourceClass != nullptr);
    }
};

static SparsePagedArray<SubtypeResourceBindings, 64> s_subtypeBindings;

void ClearSubtypeBindings()
{
    s_subtypeBindings.Clear(/* freeMemory */ true);
}

static inline SubtypeResourceBindings& GetSubtypeBindings(const Class* cls)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(cls != nullptr);

    int staticIndex = cls->GetStaticIndex();
    AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *cls->GetName());

    SubtypeResourceBindings* bindings = s_subtypeBindings.TryGet(staticIndex);
    AssertDebug(bindings != nullptr, "No SubtypeBindings container found for {}", cls->GetName());

    return *bindings;
}

void AssignResourceBinding(ObjectBase* resource, uint32 binding)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

    ObjIdBase resourceId = resource->Id();
    AssertDebug(resourceId.IsValid());

    if (binding == ~0u)
    {
        bindings.bindingIndices.EraseAt(resourceId.ToIndex());

        return;
    }

    if (bindings.gpuBufferHolder != nullptr)
    {
        bindings.gpuBufferHolder->EnsureCapacity(binding);
    }

    bindings.bindingIndices.Emplace(resourceId.ToIndex(), binding);
}

uint32 RetrieveResourceBinding(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!resource)
    {
        return ~0u; // invalid resource
    }

    const SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

    const ObjIdBase resourceId = resource->Id();

    const uint32* elem = bindings.bindingIndices.TryGet(resourceId.ToIndex());

    return elem ? *elem : ~0u;
}

#pragma endregion ResourceBindings

} // namespace Hyperion