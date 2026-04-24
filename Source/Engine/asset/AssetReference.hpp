/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetPath.hpp>

#include <Core/reflection/ObjectFwd.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Variant.hpp>

#include <Core/name/Name.hpp>

namespace Hyperion {

class AssetObject;

HYP_STRUCT()
class AssetReference
{
public:
    HYP_STRUCT_BODY(AssetReference)

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

    HYP_FORCE_INLINE bool operator==(const AssetReference& other) const
    {
        if (other.IsValid() != IsValid())
        {
            return false;
        }

        if (!IsValid())
        {
            return true;
        }

        return GetAssetPath() == other.GetAssetPath();
    }

    HYP_FORCE_INLINE bool operator!=(const AssetReference& other) const
    {
        return !(*this == other);
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

    HYP_FORCE_INLINE Name GetName() const
    {
        return GetAssetPath().GetName();
    }

    const Handle<AssetObject>& Resolve() const;
    void Reload();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return GetAssetPath().GetHashCode();
    }

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

    HYP_FORCE_INLINE const Handle<T>& Resolve() const
    {
        const Handle<AssetObject>& assetObject = AssetReference::Resolve();

        return DynamicCast<T>(assetObject);
    }
};

extern const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference);

} // namespace Hyperion
