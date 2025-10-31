#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ScriptComponentFlags Reflection Data

HYP_BEGIN_ENUM(ScriptComponentFlags, 396, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), ScriptComponentFlags::NONE),
    HypConstant(NAME(HYP_STR(INITIALIZED)), ScriptComponentFlags::INITIALIZED),
    HypConstant(NAME(HYP_STR(RELOADING)), ScriptComponentFlags::RELOADING),
    HypConstant(NAME(HYP_STR(INITIALIZATION_STARTED)), ScriptComponentFlags::INITIALIZATION_STARTED),
    HypConstant(NAME(HYP_STR(BEFORE_ADDED_CALLED)), ScriptComponentFlags::BEFORE_ADDED_CALLED),
    HypConstant(NAME(HYP_STR(ON_ADDED_CALLED)), ScriptComponentFlags::ON_ADDED_CALLED)
HYP_END_ENUM

#pragma endregion ScriptComponentFlags Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region ScriptComponent Reflection Data

HYP_BEGIN_STRUCT(ScriptComponent, 397, 0, {}, HypClassAttribute("component", true),HypClassAttribute("noscriptbindings", true),HypClassAttribute("label", "Script Component"),HypClassAttribute("description", "A script component that can be attached to an entity."))
    HypField(NAME(HYP_STR(AssetReference)), &ScriptComponent::assetReference, offsetof(ScriptComponent, assetReference), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Assembly)), &ScriptComponent::assembly, offsetof(ScriptComponent, assembly), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(ScriptObjectResource)), &ScriptComponent::scriptObjectResource, offsetof(ScriptComponent, scriptObjectResource), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(NativeObject)), &ScriptComponent::nativeObject, offsetof(ScriptComponent, nativeObject), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Flags)), &ScriptComponent::flags, offsetof(ScriptComponent, flags), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetAssetReference)), &ScriptComponent::GetAssetReference, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetReference") } }),
    HypMethod(NAME(HYP_STR(SetAssetReference)), &ScriptComponent::SetAssetReference, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetReference") } })
HYP_END_STRUCT

#pragma endregion ScriptComponent Reflection Data

HYP_REGISTER_COMPONENT(ScriptComponent);
} // namespace hyperion

