/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetRegistry.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_API WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage> &package)
{
    if (!package.IsValid()) {
        return { };
    }

    return package->GetName();
}

#pragma region AssetPackage

AssetPackage::AssetPackage()
{
}

AssetPackage::AssetPackage(Name name)
    : m_name(name)
{
}

void AssetPackage::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();
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

    if (path_string_split.Any()) {
        // Remove the actual asset name from the path
        asset_name = path_string_split.PopBack();
    }

    path_string = String::Join(path_string_split, '/');

    Mutex::Guard guard1(m_mutex);
    const Handle<AssetPackage> &asset_package = GetPackageFromPath_Internal(path_string, /* create_if_not_exist */ true);

    Mutex::Guard guard2(asset_package->m_mutex);
    asset_package->m_assets.PushBack({ std::move(asset_name), std::move(object) });
}

AnyHandle AssetRegistry::GetAsset(const UTF8StringView &path)
{
    Mutex::Guard guard1(m_mutex);
    const Handle<AssetPackage> &asset_package = GetPackageFromPath_Internal(path, /* create_if_not_exist */ false);

    if (!asset_package) {
        return AnyHandle { };
    }

    Mutex::Guard guard2(asset_package->m_mutex);
    auto it = asset_package->m_assets.FindIf([path](const Pair<String, AnyHandle> &pair)
    {
        return pair.first == path;
    });

    if (it == asset_package->m_assets.End()) {
        return AnyHandle { };
    }

    return it->second;
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView &path)
{
    Mutex::Guard guard(m_mutex);

    return GetPackageFromPath_Internal(path, /* create_if_not_exist */ false);
}

const Handle<AssetPackage> &AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView &path, bool create_if_not_exist)
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

    return GetSubpackage(current_package, current_string);
}

#pragma endregion AssetRegistry

} // namespace hyperion