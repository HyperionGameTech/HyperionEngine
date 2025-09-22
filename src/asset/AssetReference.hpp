/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>

#include <core/object/HypObject.hpp>
#include <core/object/Handle.hpp>

#include <core/utilities/Variant.hpp>

#include <core/Name.hpp>

namespace hyperion {

class AssetObject;

HYP_STRUCT()
class HYP_API AssetReference final
{
public:
    AssetReference()
        : m_data(AssetPath())
    {
    }

    explicit AssetReference(const AssetPath& assetPath)
        : m_data(assetPath)
    {
    }

    explicit AssetReference(AssetPath&& assetPath)
        : m_data(std::move(assetPath))
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

    bool IsValid() const
    {
        bool isValid = false;

        m_data.Visit([&isValid](const auto& value)
        {
            isValid = bool(value);
        });

        return isValid;
    }

    HYP_FORCE_INLINE bool IsLoaded() const
    {
        return m_data.Is<Handle<AssetObject>>();
    }

    HYP_METHOD(Property = "AssetPath", Serialize)
    const AssetPath& GetAssetPath() const;

    /*! \internal Serialization only */
    HYP_METHOD(Property = "AssetPath", Serialize)
    void SetAssetPath(const AssetPath& assetPath)
    {
        m_data = assetPath;
    }

    AssetObject* operator->() const;
    AssetObject& operator*() const;

    const Handle<AssetObject>& Resolve() const;

private:
    mutable Variant<AssetPath, Handle<AssetObject>> m_data;
};

extern const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference);

} // namespace hyperion
