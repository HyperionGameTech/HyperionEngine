#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region MeshComponent Reflection Data

HYP_BEGIN_STRUCT(MeshComponent, 391, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 240),HypClassAttribute("label", "Mesh Component"),HypClassAttribute("description", "Controls the rendering of an entity, including the mesh, material, and skeleton."),HypClassAttribute("editor", true))
    HypField(NAME(HYP_STR(Mesh)), &MeshComponent::mesh, offsetof(MeshComponent, mesh), Span<const HypClassAttribute> { {HypClassAttribute("property", "Mesh"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Material)), &MeshComponent::material, offsetof(MeshComponent, material), Span<const HypClassAttribute> { {HypClassAttribute("property", "Material"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Skeleton)), &MeshComponent::skeleton, offsetof(MeshComponent, skeleton), Span<const HypClassAttribute> { {HypClassAttribute("property", "Skeleton"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(InstanceData)), &MeshComponent::instanceData, offsetof(MeshComponent, instanceData), Span<const HypClassAttribute> { {HypClassAttribute("property", "InstanceData"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(PreviousModelMatrix)), &MeshComponent::previousModelMatrix, offsetof(MeshComponent, previousModelMatrix), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(UserData)), &MeshComponent::userData, offsetof(MeshComponent, userData), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(LightmapVolume)), &MeshComponent::lightmapVolume, offsetof(MeshComponent, lightmapVolume), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(LightmapVolumeUuid)), &MeshComponent::lightmapVolumeUuid, offsetof(MeshComponent, lightmapVolumeUuid), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(LightmapElementId)), &MeshComponent::lightmapElementId, offsetof(MeshComponent, lightmapElementId), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion MeshComponent Reflection Data

HYP_REGISTER_COMPONENT(MeshComponent);
static_assert(sizeof(MeshComponent) == 240, "Expected sizeof(MeshComponent) to be 240 bytes");
} // namespace hyperion

