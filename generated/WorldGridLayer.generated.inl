#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region WorldGridLayerInfo Reflection Data

HYP_BEGIN_STRUCT(WorldGridLayerInfo, 414, 0, {}, HypClassAttribute("size", 80))
    HypField(NAME(HYP_STR(GridSize)), &WorldGridLayerInfo::gridSize, offsetof(WorldGridLayerInfo, gridSize), Span<const HypClassAttribute> { {HypClassAttribute("property", "GridSize"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(CellSize)), &WorldGridLayerInfo::cellSize, offsetof(WorldGridLayerInfo, cellSize), Span<const HypClassAttribute> { {HypClassAttribute("property", "CellSize"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Offset)), &WorldGridLayerInfo::offset, offsetof(WorldGridLayerInfo, offset), Span<const HypClassAttribute> { {HypClassAttribute("property", "Offset"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Scale)), &WorldGridLayerInfo::scale, offsetof(WorldGridLayerInfo, scale), Span<const HypClassAttribute> { {HypClassAttribute("property", "Scale"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(MaxDistance)), &WorldGridLayerInfo::maxDistance, offsetof(WorldGridLayerInfo, maxDistance), Span<const HypClassAttribute> { {HypClassAttribute("property", "MaxDistance"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion WorldGridLayerInfo Reflection Data

static_assert(sizeof(WorldGridLayerInfo) == 80, "Expected sizeof(WorldGridLayerInfo) to be 80 bytes");
} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region WorldGridLayer Reflection Data

HYP_BEGIN_CLASS(WorldGridLayer, 192, 1, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetLayerInfo)), &WorldGridLayer::GetLayerInfo),
    HypMethod(NAME(HYP_STR(OnAdded)), &WorldGridLayer::OnAdded, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(OnRemoved)), &WorldGridLayer::OnRemoved, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(CreateStreamingCell)), &WorldGridLayer::CreateStreamingCell, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(Init)), &WorldGridLayer::Init, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(OnAdded_Impl)), &WorldGridLayer::OnAdded_Impl),
    HypMethod(NAME(HYP_STR(OnRemoved_Impl)), &WorldGridLayer::OnRemoved_Impl),
    HypMethod(NAME(HYP_STR(CreateStreamingCell_Impl)), &WorldGridLayer::CreateStreamingCell_Impl),
    HypMethod(NAME(HYP_STR(CreateLayerInfo)), &WorldGridLayer::CreateLayerInfo, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(CreateLayerInfo_Impl)), &WorldGridLayer::CreateLayerInfo_Impl),
    HypMethod(NAME(HYP_STR(Init_Impl)), &WorldGridLayer::Init_Impl)
HYP_END_CLASS

#pragma endregion WorldGridLayer Reflection Data

#pragma region WorldGridLayer Scriptable Methods

void WorldGridLayer::OnAdded(WorldGrid * worldGrid)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnAdded");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, worldGrid);
            return;
        }
    }

    OnAdded_Impl(worldGrid);
}
void WorldGridLayer::OnRemoved(WorldGrid * worldGrid)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnRemoved");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, worldGrid);
            return;
        }
    }

    OnRemoved_Impl(worldGrid);
}
Handle<StreamingCell> WorldGridLayer::CreateStreamingCell(const StreamingCellInfo & cellInfo)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("CreateStreamingCell");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Handle<StreamingCell>>(method_ptr, cellInfo);
        }
    }

    return CreateStreamingCell_Impl(cellInfo);
}
void WorldGridLayer::Init()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Init");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    Init_Impl();
}
WorldGridLayerInfo WorldGridLayer::CreateLayerInfo() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("CreateLayerInfo");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<WorldGridLayerInfo>(method_ptr);
        }
    }

    return CreateLayerInfo_Impl();
}
#pragma endregion WorldGridLayer Scriptable Methods
} // namespace hyperion

