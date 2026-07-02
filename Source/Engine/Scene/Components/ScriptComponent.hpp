/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Utilities/EnumFlags.hpp>

#include <Scripting/Script.hpp>
#include <Scripting/ScriptObjectResource.hpp>
#include <Scripting/Asset/ScriptAsset.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedObject;
class Assembly;
} // namespace dotnet

HYP_ENUM()
enum class ScriptComponentFlags : uint32
{
    NONE = 0x0,
    INITIALIZED = 0x1,
    RELOADING = 0x2,
    INITIALIZATION_STARTED = 0x4,
    ACTIVATED = 0x8
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

HYP_STRUCT(Component, NoScriptBindings, Label = "Script Component", Description = "A script component that can be attached to an entity.")
struct ScriptComponent
{
    HYP_STRUCT_BODY(ScriptComponent);

    HYP_FIELD(Property = "Script")
    Handle<ScriptAsset> script;

    HYP_FIELD(NoScriptBindings, Transient)
    SharedPtr<dotnet::Assembly> assembly;

    HYP_FIELD(NoScriptBindings, Transient)
    ScriptObjectResource* scriptObjectResource = nullptr;

    HYP_FIELD(NoScriptBindings, Transient)
    Handle<ObjectBase> nativeObject;

    HYP_FIELD(Transient)
    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;
};

} // namespace Hyperion
