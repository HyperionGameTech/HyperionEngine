/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class AssetObject;

HYP_CLASS(Abstract)
class ENGINE_API AssetExtension : public ObjectBase
{
    HYP_OBJECT_BODY(AssetExtension);

public:
    friend class AssetObject;

    ~AssetExtension() override = default;

    HYP_METHOD()
    Handle<AssetObject> GetOwner() const;

    HYP_METHOD()
    void NotifyChanged();

    static bool CanExtend(const Class* extensionClass, const Class* assetClass);

    static Array<const Class*> GetExtensionClasses(const Class* assetClass);

    static Handle<AssetExtension> Create(const Class* extensionClass);

protected:
    /// fills in a new extension from its asset, eg data migrated from an older format or derived from the asset
    virtual void InitForAsset(const AssetObject& asset)
    {
    }

private:
    WeakHandle<AssetObject> m_owner;
};

} // namespace Hyperion
