/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <scripting/Script.hpp>

namespace hyperion {

HYP_CLASS()
class ScriptAsset : public AssetObject
{
    HYP_OBJECT_BODY(ScriptAsset);

public:
    ScriptAsset()
        : AssetObject()
    {
        AssetObject::SetData(ScriptData());
    }

    ScriptAsset(Name name, const ScriptData& scriptData)
        : AssetObject(name, scriptData)
    {
    }

    ScriptAsset(Name name, ScriptData&& scriptData)
        : AssetObject(name, std::move(scriptData))
    {
    }

    ScriptAsset(const ScriptAsset& other) = delete;
    ScriptAsset& operator=(const ScriptAsset& other) = delete;

    ScriptAsset(ScriptAsset&& other) noexcept = delete;
    ScriptAsset& operator=(ScriptAsset&& other) noexcept = delete;

    ~ScriptAsset() = default;

    HYP_FORCE_INLINE ScriptData* GetScriptData() const
    {
        return GetResourceData<ScriptData>();
    }
};

} // namespace hyperion
