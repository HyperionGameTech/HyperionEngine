/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ASSET_REGISTRY_HPP
#define HYPERION_ASSET_REGISTRY_HPP

#include <util/fs/FsUtil.hpp>

#include <core/Base.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/UUID.hpp>
#include <core/utilities/StringView.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/Defines.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/util/ForEach.hpp>

#include <asset/AssetLoader.hpp>

#include <Constants.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetRegistry;
class AssetPackage;

HYP_API extern WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage> &package);

using AssetPackageSet = HashSet<Handle<AssetPackage>, &AssetPackage_KeyByFunction>;

HYP_CLASS()
class HYP_API AssetPackage : public HypObject<AssetPackage>
{
    HYP_OBJECT_BODY(AssetPackage);

    friend class AssetRegistry;

public:
    AssetPackage();
    AssetPackage(Name name);
    AssetPackage(const AssetPackage &other)                 = delete;
    AssetPackage &operator=(const AssetPackage &other)      = delete;
    AssetPackage(AssetPackage &&other) noexcept             = delete;
    AssetPackage &operator=(AssetPackage &&other) noexcept  = delete;
    ~AssetPackage()                                         = default;

    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE const UUID &GetUUID() const
        { return m_uuid; }

    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE void SetUUID(const UUID &uuid)
        { m_uuid = uuid; }

    HYP_METHOD(Property="Name", Serialize=true)
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_METHOD(Property="Name", Serialize=true)
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

    HYP_FORCE_INLINE const WeakHandle<AssetPackage> &GetParentPackage() const
        { return m_parent_package; }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE const AssetPackageSet &GetSubpackages() const
        { return m_subpackages; }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE void SetSubpackages(const AssetPackageSet &subpackages)
        { m_subpackages = subpackages; }

    HYP_METHOD(Property="Assets", Serialize=true)
    HYP_FORCE_INLINE const Array<Pair<String, AnyHandle>> &GetAssets() const
        { return m_assets; }

    /*! \internal Serialization only */
    HYP_METHOD(Property="Assets", Serialize=true)
    HYP_FORCE_INLINE void SetAssets(const Array<Pair<String, AnyHandle>> &assets)
        { m_assets = assets; }

    void Init();

private:
    UUID                            m_uuid;
    Name                            m_name;
    WeakHandle<AssetPackage>        m_parent_package;
    AssetPackageSet                 m_subpackages;
    Array<Pair<String, AnyHandle>>  m_assets;
    Mutex                           m_mutex;
};


HYP_CLASS()
class HYP_API AssetRegistry : public HypObject<AssetRegistry>
{
    HYP_OBJECT_BODY(AssetRegistry);

public:
    AssetRegistry();
    AssetRegistry(const AssetRegistry &other)                   = delete;
    AssetRegistry &operator=(const AssetRegistry &other)        = delete;
    AssetRegistry(AssetRegistry &&other) noexcept               = delete;
    AssetRegistry &operator=(AssetRegistry &&other) noexcept    = delete;
    ~AssetRegistry()                                            = default;

    HYP_FORCE_INLINE const AssetPackageSet &GetPackages() const
        { return m_packages; }

    /*! \internal Serialization only */
    void SetPackages(const AssetPackageSet &packages);

    template <class Callback>
    void ForEachPackage(Callback &&callback) const
        { ForEach(m_packages, m_mutex, std::forward<Callback>(callback)); }

    void RegisterAsset(const UTF8StringView &path, AnyHandle &&object);

    HYP_METHOD()
    AnyHandle GetAsset(const UTF8StringView &path);

    HYP_METHOD()
    Handle<AssetPackage> GetPackageFromPath(const UTF8StringView &path);

    void Init();

    Delegate<void, const Handle<AssetPackage> &>    OnPackageAdded;

private:
    const Handle<AssetPackage> &GetPackageFromPath_Internal(const UTF8StringView &path, bool create_if_not_exist);

    AssetPackageSet m_packages;
    mutable Mutex   m_mutex;
};

} // namespace hyperion

#endif
