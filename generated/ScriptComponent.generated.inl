#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ScriptComponentFlags Reflection Data

HYP_BEGIN_ENUM(ScriptComponentFlags, 388, 0, {})
    StaticField(NAME(HYP_STR(NONE)), ScriptComponentFlags::NONE),
    StaticField(NAME(HYP_STR(INITIALIZED)), ScriptComponentFlags::INITIALIZED),
    StaticField(NAME(HYP_STR(RELOADING)), ScriptComponentFlags::RELOADING),
    StaticField(NAME(HYP_STR(INITIALIZATION_STARTED)), ScriptComponentFlags::INITIALIZATION_STARTED),
    StaticField(NAME(HYP_STR(BEFORE_ADDED_CALLED)), ScriptComponentFlags::BEFORE_ADDED_CALLED),
    StaticField(NAME(HYP_STR(ON_ADDED_CALLED)), ScriptComponentFlags::ON_ADDED_CALLED)
HYP_END_ENUM

#pragma endregion ScriptComponentFlags Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region ScriptComponent Reflection Data

HYP_BEGIN_STRUCT(ScriptComponent, 389, 0, {}, ClassAttribute("component", true),ClassAttribute("noscriptbindings", true),ClassAttribute("label", "Script Component"),ClassAttribute("description", "A script component that can be attached to an entity."))
    Field(NAME(HYP_STR(AssetReference)), &ScriptComponent::assetReference, offsetof(ScriptComponent, assetReference), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Assembly)), &ScriptComponent::assembly, offsetof(ScriptComponent, assembly), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(ScriptObjectResource)), &ScriptComponent::scriptObjectResource, offsetof(ScriptComponent, scriptObjectResource), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(NativeObject)), &ScriptComponent::nativeObject, offsetof(ScriptComponent, nativeObject), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Flags)), &ScriptComponent::flags, offsetof(ScriptComponent, flags), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetAssetReference)), &ScriptComponent::GetAssetReference, Span<const ClassAttribute> { {ClassAttribute("property", "AssetReference") } }),
    Method(NAME(HYP_STR(SetAssetReference)), &ScriptComponent::SetAssetReference, Span<const ClassAttribute> { {ClassAttribute("property", "AssetReference") } })
HYP_END_STRUCT

#pragma endregion ScriptComponent Reflection Data

HYP_REGISTER_COMPONENT(ScriptComponent);
} // namespace hyperion

