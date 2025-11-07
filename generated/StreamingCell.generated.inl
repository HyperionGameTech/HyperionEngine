#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region StreamingCellNeighbor Reflection Data

HYP_BEGIN_STRUCT(StreamingCellNeighbor, 275, 0, {})
    Field(NAME(HYP_STR(Coord)), &StreamingCellNeighbor::coord, offsetof(StreamingCellNeighbor, coord), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Coord") } })
HYP_END_STRUCT

#pragma endregion StreamingCellNeighbor Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StreamingCellInfo Reflection Data

HYP_BEGIN_STRUCT(StreamingCellInfo, 276, 0, {}, ClassAttribute("size", 80))
    Field(NAME(HYP_STR(Coord)), &StreamingCellInfo::coord, offsetof(StreamingCellInfo, coord), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Coord") } }),
    Field(NAME(HYP_STR(Extent)), &StreamingCellInfo::extent, offsetof(StreamingCellInfo, extent), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Extent") } }),
    Field(NAME(HYP_STR(Scale)), &StreamingCellInfo::scale, offsetof(StreamingCellInfo, scale), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Scale") } }),
    Field(NAME(HYP_STR(Bounds)), &StreamingCellInfo::bounds, offsetof(StreamingCellInfo, bounds), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Bounds") } })
HYP_END_STRUCT

#pragma endregion StreamingCellInfo Reflection Data

static_assert(sizeof(StreamingCellInfo) == 80, "Expected sizeof(StreamingCellInfo) to be 80 bytes");
} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region StreamingCell Reflection Data

HYP_BEGIN_CLASS(StreamingCell, 65, 1, NAME("StreamableBase"))
    Method(NAME(HYP_STR(GetPatchInfo)), &StreamingCell::GetPatchInfo),
    Method(NAME(HYP_STR(Update)), &StreamingCell::Update, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(GetBoundingBox_Impl)), &StreamingCell::GetBoundingBox_Impl),
    Method(NAME(HYP_STR(Update_Impl)), &StreamingCell::Update_Impl)
HYP_END_CLASS

#pragma endregion StreamingCell Reflection Data

#pragma region StreamingCell Scriptable Methods

void StreamingCell::Update(float delta)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Update");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, delta);
            return;
        }
    }

    Update_Impl(delta);
}
#pragma endregion StreamingCell Scriptable Methods
} // namespace hyperion


namespace hyperion {

#pragma region StreamingCellState Reflection Data

HYP_BEGIN_ENUM(StreamingCellState, 277, 0, {})
    StaticField(NAME(HYP_STR(INVALID)), StreamingCellState::INVALID),
    StaticField(NAME(HYP_STR(UNLOADED)), StreamingCellState::UNLOADED),
    StaticField(NAME(HYP_STR(UNLOADING)), StreamingCellState::UNLOADING),
    StaticField(NAME(HYP_STR(WAITING)), StreamingCellState::WAITING),
    StaticField(NAME(HYP_STR(LOADING)), StreamingCellState::LOADING),
    StaticField(NAME(HYP_STR(LOADED)), StreamingCellState::LOADED),
    StaticField(NAME(HYP_STR(MAX)), StreamingCellState::MAX)
HYP_END_ENUM

#pragma endregion StreamingCellState Reflection Data

} // namespace hyperion

