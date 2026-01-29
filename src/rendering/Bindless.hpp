/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjId.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/IdGenerator.hpp>

#include <core/containers/HashMap.hpp>

#include <rendering/RenderObject.hpp>

#include <engine/EngineMemory.hpp>

namespace Hyperion {

class Texture;

enum BindlessStorageSlot : uint8
{
    BindlessStorage_Slot0,
    BindlessStorage_Slot1,

    BindlessStorage_Max
};

static constexpr StringHash BindlessStorageSlotNames[BindlessStorage_Max] = {
    "BindlessResources0"_sh,
    "BindlessResources1"_sh
};

static constexpr StringHash BindlessStorageDescriptorNames[BindlessStorage_Max] = {
    "Textures"_sh,
    "Buffers"_sh
};

class BindlessStorage
{
public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage& other) = delete;
    BindlessStorage& operator=(const BindlessStorage& other) = delete;
    BindlessStorage(BindlessStorage&& other) noexcept = delete;
    BindlessStorage& operator=(BindlessStorage&& other) noexcept = delete;
    ~BindlessStorage();

    void UnsetAllResources(BindlessStorageSlot slot);

    /*! \brief Add a resource to the bindless storage slot \p slot. */
    void AddResource(BindlessStorageSlot slot, uint32 index, const Handle<ObjectBase>& resource);

    /*! \brief Remove the given resource from the bindless storage slot designated to \p slot */
    void RemoveResource(BindlessStorageSlot slot, uint32 index);

private:
    using ResourceList = SparsePagedArray<WeakHandle<ObjectBase>, 256, RenderAllocator>;
    ResourceList m_resources[BindlessStorage_Max];
};

} // namespace Hyperion
