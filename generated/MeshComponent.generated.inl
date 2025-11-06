#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region MeshComponent Reflection Data

HYP_BEGIN_STRUCT(MeshComponent, 383, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 240),ClassAttribute("label", "Mesh Component"),ClassAttribute("description", "Controls the rendering of an entity, including the mesh, material, and skeleton."),ClassAttribute("editor", true))
    Field(NAME(HYP_STR(Mesh)), &MeshComponent::mesh, offsetof(MeshComponent, mesh), Span<const ClassAttribute> { {ClassAttribute("property", "Mesh"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Material)), &MeshComponent::material, offsetof(MeshComponent, material), Span<const ClassAttribute> { {ClassAttribute("property", "Material"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Skeleton)), &MeshComponent::skeleton, offsetof(MeshComponent, skeleton), Span<const ClassAttribute> { {ClassAttribute("property", "Skeleton"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(InstanceData)), &MeshComponent::instanceData, offsetof(MeshComponent, instanceData), Span<const ClassAttribute> { {ClassAttribute("property", "InstanceData"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(PreviousModelMatrix)), &MeshComponent::previousModelMatrix, offsetof(MeshComponent, previousModelMatrix), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(UserData)), &MeshComponent::userData, offsetof(MeshComponent, userData), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(LightmapVolume)), &MeshComponent::lightmapVolume, offsetof(MeshComponent, lightmapVolume), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(LightmapVolumeUuid)), &MeshComponent::lightmapVolumeUuid, offsetof(MeshComponent, lightmapVolumeUuid), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(LightmapElementId)), &MeshComponent::lightmapElementId, offsetof(MeshComponent, lightmapElementId), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion MeshComponent Reflection Data

HYP_REGISTER_COMPONENT(MeshComponent);
static_assert(sizeof(MeshComponent) == 240, "Expected sizeof(MeshComponent) to be 240 bytes");
} // namespace hyperion

