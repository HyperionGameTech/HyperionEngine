/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/IdGenerator.hpp>

#include <Core/containers/HashMap.hpp>

#include <rendering/RenderObject.hpp>

#include <engine/EngineMemory.hpp>

namespace Hyperion {

class Texture;

enum BindlessStorageSlot : uint8
{
    BindlessStorage_Textures,
    BindlessStorage_Buffers,

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

static constexpr uint32 MaxBindlessResources[BindlessStorage_Max] = {
    2048,
    16384
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

    HYP_NODISCARD uint32 AllocateId(BindlessStorageSlot slot)
    {
        return m_idGenerators[slot].Next() - 1;
    }

    void ReleaseId(BindlessStorageSlot slot, uint32 id)
    {
        m_idGenerators[slot].ReleaseId(id + 1);
    }

    void UnsetAllResources(BindlessStorageSlot slot);

    /*! \brief Add a resource to the bindless storage slot \p slot. */
    void AddResource(BindlessStorageSlot slot, uint32 index, const Handle<ObjectBase>& resource);

    /*! \brief Remove the given resource from the bindless storage slot designated to \p slot */
    void RemoveResource(BindlessStorageSlot slot, uint32 index);

private:
    using ResourceList = SparsePagedArray<WeakHandle<ObjectBase>, 256, RenderAllocator>;
    ResourceList m_resources[BindlessStorage_Max];

    IdGenerator m_idGenerators[BindlessStorage_Max];
};

} // namespace Hyperion
