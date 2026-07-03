/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetPath.hpp>

#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Variant.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class AssetObject;

HYP_STRUCT()
class ENGINE_API AssetReference
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

    HYP_METHOD(NoScriptBindings)
    bool IsValid() const
    {
        bool isValid = false;

        m_data.Visit([&isValid](const auto& value)
            {
                isValid = bool(value);
            });

        return isValid;
    }

    HYP_METHOD(NoScriptBindings)
    bool IsLoaded() const
    {
        return m_data.Is<Handle<AssetObject>>();
    }

    HYP_METHOD(Property = "AssetPath", NoScriptBindings, Serialize)
    const AssetPath& GetAssetPath() const;

    /*! \internal Serialization only */
    HYP_METHOD(Property = "AssetPath", NoScriptBindings, Serialize)
    void SetAssetPath(const AssetPath& assetPath)
    {
        m_data = assetPath;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return GetAssetPath().GetName();
    }

    HYP_METHOD(NoScriptBindings)
    const Handle<AssetObject>& Resolve() const;

    HYP_METHOD(NoScriptBindings)
    void Reload();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return GetAssetPath().GetHashCode();
    }

private:
    mutable Variant<AssetPath, Handle<AssetObject>> m_data;
};

} // namespace Hyperion
