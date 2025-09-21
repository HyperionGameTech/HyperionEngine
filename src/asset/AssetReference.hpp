/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>

#include <core/object/HypObject.hpp>

#include <core/Name.hpp>

namespace hyperion {

class AssetObject;

HYP_STRUCT()
class HYP_API AssetReference
{
public:
    AssetReference() = default;

    explicit AssetReference(const AssetPath& assetPath)
        : assetPath(assetPath)
    {
    }

    explicit AssetReference(AssetPath&& assetPath)
        : assetPath(std::move(assetPath))
    {
    }

    explicit AssetReference(const Handle<AssetObject>& assetObject);

    AssetReference(const AssetReference& other) = default;
    AssetReference& operator=(const AssetReference& other) = default;

    AssetReference(AssetReference&& other) noexcept = default;
    AssetReference& operator=(AssetReference&& other) noexcept = default;

    ~AssetReference() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return assetPath.IsValid();
    }

    HYP_FORCE_INLINE bool IsLoaded() const
    {
        return assetObject.IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const AssetReference& other) const
    {
        return assetPath.ToString() == other.assetPath.ToString();
    }

    HYP_FORCE_INLINE bool operator!=(const AssetReference& other) const
    {
        return assetPath.ToString() != other.assetPath.ToString();
    }

    AssetObject* operator->() const;
    AssetObject& operator*() const;

    const Handle<AssetObject>& Resolve() const;

    HYP_FIELD(Serialize)
    AssetPath assetPath;

    HYP_FIELD(Serialize = false)
    mutable Handle<AssetObject> assetObject;
};

} // namespace hyperion
