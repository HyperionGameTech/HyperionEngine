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
class HYP_API AssetReference
{
public:
    STRUCT_BODY(AssetReference)

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

template <class T>
class TAssetReference : public AssetReference
{
public:
    TAssetReference() = default;

    explicit TAssetReference(const AssetReference& assetReference)
        : AssetReference(assetReference)
    {
    }

    explicit TAssetReference(AssetReference&& assetReference) noexcept
        : AssetReference(std::move(assetReference))
    {
    }

    explicit TAssetReference(const AssetPath& assetPath)
        : AssetReference(assetPath)
    {
    }

    explicit TAssetReference(AssetPath&& assetPath)
        : AssetReference(std::move(assetPath))
    {
    }

    explicit TAssetReference(const Handle<T>& assetObject)
        : AssetReference(assetObject)
    {
    }

    TAssetReference(const TAssetReference& other) = default;
    TAssetReference& operator=(const TAssetReference& other) = default;

    TAssetReference(TAssetReference&& other) noexcept = default;
    TAssetReference& operator=(TAssetReference&& other) noexcept = default;

    ~TAssetReference() = default;

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
        return AssetReference::IsValid();
    }

    HYP_FORCE_INLINE bool IsLoaded() const
    {
        return AssetReference::IsLoaded();
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return static_cast<T*>(AssetReference::operator->());
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return static_cast<T&>(AssetReference::operator*());
    }

    HYP_FORCE_INLINE const Handle<T>& Resolve() const
    {
        const Handle<AssetObject>& assetObject = AssetReference::Resolve();

        return ObjCast<T>(assetObject);
    }
};

extern const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference);

} // namespace hyperion
