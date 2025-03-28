/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetRegistry.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_API WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage> &package)
{
    if (!package.IsValid()) {
        return { };
    }

    return package->GetName();
}

HYP_API WeakName AssetObject_KeyByFunction(const Handle<AssetObject> &asset_object)
{
    if (!asset_object.IsValid()) {
        return { };
    }

    return asset_object->GetName();
}

#pragma region AssetObject

AssetObject::AssetObject()
    : AssetObject(Name::Invalid())
{
}

AssetObject::AssetObject(Name name)
    : AssetObject(name, AnyHandle { })
{
}

AssetObject::AssetObject(Name name, AnyHandle &&value)
    : m_name(name),
      m_value(std::move(value))
{
}

void AssetObject::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();
}

#pragma endregion AssetObject

#pragma region AssetPackage

AssetPackage::AssetPackage()
{
}

AssetPackage::AssetPackage(Name name)
    : m_name(name)
{
}

void AssetPackage::SetAssetObjects(const AssetObjectSet &asset_objects)
{
    if (IsInitCalled()) {
        AssetObjectSet previous_asset_objects;

        { // store so we can call OnAssetObjectRemoved outside of the lock
            Mutex::Guard guard(m_mutex);

            previous_asset_objects = std::move(m_asset_objects);
        }

        for (const Handle<AssetObject> &asset_object : previous_asset_objects) {
            OnAssetObjectRemoved(asset_object.Get());
        }

        previous_asset_objects.Clear();
    }

    Array<Handle<AssetObject>> new_asset_objects;

    {
        Mutex::Guard guard(m_mutex);

        m_asset_objects = asset_objects;

        if (IsInitCalled()) {
            new_asset_objects.Reserve(m_asset_objects.Size());

            for (const Handle<AssetObject> &asset_object : m_asset_objects) {
                InitObject(asset_object);

                new_asset_objects.PushBack(asset_object);
            }
        }
    }

    for (const Handle<AssetObject> &asset_object : new_asset_objects) {
        OnAssetObjectAdded(asset_object.Get());
    }
}

Handle<AssetObject> AssetPackage::NewAssetObject(Name name, const AnyHandle &value)
{
    Handle<AssetObject> asset_object;

    { // so we can call OnAssetObjectAdded outside of the lock
        Mutex::Guard guard(m_mutex);

        asset_object = CreateObject<AssetObject>(name, AnyHandle(value));
        m_asset_objects.Insert({ asset_object });

        if (IsInitCalled()) {
            InitObject(asset_object);
        }
    }

    if (IsInitCalled()) {
        OnAssetObjectAdded(asset_object.Get());
    }

    return asset_object;
}

void AssetPackage::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    Array<Handle<AssetObject>> asset_objects;
    Array<Handle<AssetPackage>> subpackages;

    {
        Mutex::Guard guard(m_mutex);

        asset_objects.Reserve(m_asset_objects.Size());
        subpackages.Reserve(m_subpackages.Size());

        for (const Handle<AssetObject> &asset_object : m_asset_objects) {
            InitObject(asset_object);

            asset_objects.PushBack(asset_object);
        }

        for (const Handle<AssetPackage> &subpackage : m_subpackages) {
            InitObject(subpackage);

            subpackages.PushBack(subpackage);
        }
    }

    for (const Handle<AssetObject> &asset_object : asset_objects) {
        OnAssetObjectAdded(asset_object.Get());
    }
}

#pragma endregion AssetPackage

#pragma region AssetRegistry

AssetRegistry::AssetRegistry()
{
    const auto AddPackage = [this](Name name, WeakName parent_package_name = Name::Invalid())
    {
        Handle<AssetPackage> package = CreateObject<AssetPackage>(name);

        if (parent_package_name.IsValid()) {
            auto parent_package_it = m_packages.Find(parent_package_name);
            AssertThrowMsg(parent_package_it != m_packages.End(), "No package with name '%s' found", Name(parent_package_name).LookupString());

            package->m_parent_package = *parent_package_it;

            (*parent_package_it)->m_subpackages.Insert(std::move(package));

            return;
        }

        // temp
        package->SetAssetObjects({
            CreateObject<AssetObject>(NAME("TestAsset1"), AnyHandle { }),
            CreateObject<AssetObject>(NAME("TestAsset2"), AnyHandle { }),
            CreateObject<AssetObject>(NAME("TestAsset3"), AnyHandle { })
        });

        m_packages.Insert(std::move(package));
    };

    AddPackage(NAME("Media"));
    AddPackage(NAME("Textures"), "Media");
    AddPackage(NAME("Models"), "Media");
    AddPackage(NAME("Materials"), "Media");
    
    AddPackage(NAME("Scripts"));
    AddPackage(NAME("Config"));
}

void AssetRegistry::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    {
        Mutex::Guard guard(m_mutex);

        for (const Handle<AssetPackage> &package : m_packages) {
            InitObject(package);
        }
    }
}

void AssetRegistry::SetPackages(const AssetPackageSet &packages)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    Proc<void, Handle<AssetPackage>> InitializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    InitializePackage = [this, &InitializePackage](const Handle<AssetPackage> &parent_package)
    {
        for (const Handle<AssetPackage> &package : parent_package->m_subpackages) {
            package->m_parent_package = parent_package;

            InitializePackage(package);
        }

        OnPackageAdded(parent_package);
    };

    for (const Handle<AssetPackage> &package : packages) {
        m_packages.Set(package);

        InitializePackage(package);
    }
}

void AssetRegistry::RegisterAsset(const UTF8StringView &path, AnyHandle &&object)
{
    HYP_SCOPE;

    String path_string = path;
    Array<String> path_string_split = path_string.Split('/', '\\');

    String asset_name;

    path_string = String::Join(path_string_split, '/');

    Mutex::Guard guard1(m_mutex);
    Handle<AssetPackage> asset_package = GetPackageFromPath_Internal(path_string, AssetRegistryPathType::PACKAGE, /* create_if_not_exist */ true, asset_name);

    asset_package->NewAssetObject(CreateNameFromDynamicString(asset_name), std::move(object));
}

const AnyHandle &AssetRegistry::GetAsset(const UTF8StringView &path)
{
    Mutex::Guard guard1(m_mutex);

    String asset_name;

    Handle<AssetPackage> asset_package = GetPackageFromPath_Internal(path, AssetRegistryPathType::ASSET, /* create_if_not_exist */ false, asset_name);

    if (!asset_package.IsValid()) {
        return AnyHandle::empty;
    }

    Mutex::Guard guard2(asset_package->m_mutex);
    auto it = asset_package->m_asset_objects.Find(WeakName(asset_name.Data()));

    if (it == asset_package->m_asset_objects.End()) {
        return AnyHandle::empty;
    }

    return (*it)->GetObject();
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView &path, bool create_if_not_exist)
{
    Mutex::Guard guard(m_mutex);

    String asset_name;

    return GetPackageFromPath_Internal(path, AssetRegistryPathType::PACKAGE, create_if_not_exist, asset_name);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView &path, AssetRegistryPathType path_type, bool create_if_not_exist, String &out_asset_name)
{
    HYP_SCOPE;

    Handle<AssetPackage> current_package;
    String current_string;

    const auto GetSubpackage = [this, create_if_not_exist](const Handle<AssetPackage> &parent_package, UTF8StringView str) -> const Handle<AssetPackage> &
    {
        AssetPackageSet *packages_set = nullptr;
        
        if (!parent_package) {
            packages_set = &m_packages;
        } else {
            packages_set = &parent_package->m_subpackages;
        }

        auto package_it = packages_set->Find(str);

        if (create_if_not_exist && package_it == packages_set->End()) {
            Handle<AssetPackage> package = CreateObject<AssetPackage>(CreateNameFromDynamicString(str.Data()));
            package->m_parent_package = parent_package;
            InitObject(package);

            package_it = packages_set->Insert(std::move(package)).first;

            OnPackageAdded(package);
        }

        return package_it != packages_set->End() ? *package_it : Handle<AssetPackage>::empty;
    };

    for (auto it = path.Begin(); it != path.End(); ++it) {
        if (*it == utf::u32char('/') || *it == utf::u32char('\\')) {
            current_package = GetSubpackage(current_package, current_string);

            current_string.Clear();

            if (!current_package) {
                return Handle<AssetPackage>::empty;
            }

            continue;
        }

        current_string.Append(*it);
    }

    switch (path_type) {
    case AssetRegistryPathType::PACKAGE:
        out_asset_name.Clear();

        // If it is a PACKAGE path, if there is any remaining string, get / create the subpackage
        if (!current_package.IsValid() || current_string.Any()) {
            current_package = GetSubpackage(current_package, current_string);
        }

        return current_package;
    case AssetRegistryPathType::ASSET:
        out_asset_name = std::move(current_string);

        return current_package;
    default:
        HYP_UNREACHABLE();
    }
}

#pragma endregion AssetRegistry

} // namespace hyperion