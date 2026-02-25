/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/utilities/EnumFlags.hpp>

#include <scripting/Script.hpp>
#include <scripting/ScriptObjectResource.hpp>

#include <Core/HashCode.hpp>

#include <asset/AssetReference.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedObject;
class Assembly;
} // namespace dotnet

class ScriptAsset;

HYP_ENUM()
enum class ScriptComponentFlags : uint32
{
    NONE = 0x0,
    INITIALIZED = 0x1,
    RELOADING = 0x2,
    INITIALIZATION_STARTED = 0x4,
    BEFORE_ADDED_CALLED = 0x10,
    ON_ADDED_CALLED = 0x20 // the script has already been compiled once, with Init() and BeforeAdded() called. don't call them again.
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

HYP_STRUCT(Component, NoScriptBindings, Label = "Script Component", Description = "A script component that can be attached to an entity.")
struct ScriptComponent
{
    HYP_STRUCT_BODY(ScriptComponent);

    HYP_FIELD(NoScriptBindings, Transient)
    TAssetReference<ScriptAsset> assetReference;

    HYP_FIELD(NoScriptBindings, Transient)
    RC<dotnet::Assembly> assembly;

    HYP_FIELD(NoScriptBindings, Transient)
    ScriptObjectResource* scriptObjectResource = nullptr;

    HYP_FIELD(NoScriptBindings, Transient)
    Handle<ObjectBase> nativeObject;

    HYP_FIELD(Transient)
    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;

    HYP_METHOD(Property = "AssetReference")
    const AssetReference& GetAssetReference() const
    {
        return assetReference;
    }

    HYP_METHOD(Property = "AssetReference")
    void SetAssetReference(const AssetReference& value)
    {
        assetReference = TAssetReference<ScriptAsset>(value);
    }
};

} // namespace Hyperion
