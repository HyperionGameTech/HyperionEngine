/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <scripting/Script.hpp>

namespace Hyperion {

HYP_CLASS()
class ScriptAsset : public AssetObject
{
    HYP_OBJECT_BODY(ScriptAsset);

public:
    ScriptAsset()
        : AssetObject()
    {
        AssetObject::SetData(ScriptDesc());
    }

    explicit ScriptAsset(Name name)
        : AssetObject(name)
    {
        AssetObject::SetData(ScriptDesc());
    }

    ScriptAsset(Name name, const ScriptDesc& scriptData)
        : AssetObject(name, scriptData)
    {
    }

    ScriptAsset(Name name, ScriptDesc&& scriptData)
        : AssetObject(name, std::move(scriptData))
    {
    }

    ScriptAsset(const ScriptAsset& other) = delete;
    ScriptAsset& operator=(const ScriptAsset& other) = delete;

    ScriptAsset(ScriptAsset&& other) noexcept = delete;
    ScriptAsset& operator=(ScriptAsset&& other) noexcept = delete;

    ~ScriptAsset() = default;

    HYP_FORCE_INLINE ScriptDesc* GetScriptData() const
    {
        return GetResourceData<ScriptDesc>();
    }
};

} // namespace Hyperion
