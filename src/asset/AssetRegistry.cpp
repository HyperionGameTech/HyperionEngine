/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetReference.hpp>
#include <asset/Assets.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>
#include <core/reflection/TypeInfo.hpp>

#include <core/threading/Scheduler.hpp>

#include <core/reflection/SerializeHelpers.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/Property.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/json/JSON.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>

#include <streaming/StreamingCell.hpp>

#include <engine/EngineDriver.hpp>

#include <AssetRegistry.generated.inl>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

static const ThreadId& s_assetRegistryThread = g_simThread;

// If true, all mutation operations will be forced to run on the sim thread,
// otherwise a mutex will be used to allow multi-threaded access.
static constexpr bool UseSingleThread = false;

static constexpr const StringHash PredefinedPackageNames[] = {
    "Engine"_sh,
    "$Memory"_sh,
    "$Temp"_sh,
    "$Import"_sh
};

static constexpr const StringHash BaseTransientPackageNames[] = {
    "$Memory"_sh,
    "$Temp"_sh,
    "$Import"_sh
};

extern HYP_API const FilePath& GetResourceDirectory();

StringHash AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage)
{
    if (!assetPackage.IsValid())
    {
        return {};
    }

    return assetPackage->GetName();
}

StringHash AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return {};
    }

    return assetObject->GetName();
}

HYP_NODISCARD String SanitizeName(const UTF8StringView& nameStr)
{
    String newString;
    newString.Reserve(nameStr.Size());

    for (auto it = nameStr.Begin(); it != nameStr.End(); ++it)
    {
        const utf::Char32 c = *it;

        if (!std::isalnum(int(c)) && c != '$' && c != '-' && c != '_')
        {
            newString.Append('_');

            continue;
        }

        newString.Append(*it);
    }

    return newString;
}

HYP_NODISCARD Name SanitizeName(Name name)
{
    if (!name.IsValid())
    {
        return Name::Invalid();
    }

    const char* str = name.LookupString();
    AssertDebug(str != nullptr);

    String newString = SanitizeName(UTF8StringView(str));

    if (newString == str)
    {
        // reuse existing name if nothing changed
        return name;
    }

    return CreateNameFromDynamicString(newString);
}

template <class T>
static Name GetUniqueName(Name baseName, T&& elements)
{
    baseName = SanitizeName(baseName);

    String str = *baseName;

    int counter = 0;
    while (elements.FindAs(StringHash(*str)) != elements.End())
    {
        counter++;

        str = HYP_FORMAT("{}{}", *baseName, counter);
    }

    if (counter > 0)
    {
        return CreateNameFromDynamicString(str);
    }

    return baseName;
}

HYP_NODISCARD Name CreateFriendlyName(Name name)
{
    if (!name.IsValid())
    {
        return Name::Invalid();
    }

    const char* str = name.LookupString();
    AssertDebug(str != nullptr);

    String friendlyNameStr;

    for (auto it : UTF8StringView(str))
    {
        if (utf::IsAlphabetical(it) || utf::IsDecimal(it))
        {
            friendlyNameStr.Append(it);
        }
    }

    return CreateNameFromDynamicString(StringUtil::ToPascalCase(friendlyNameStr, true));
}

static bool IsTransientAssetPath(const AssetPath& assetPath)
{
    if (!assetPath)
    {
        return false;
    }

    Name rootPackageName = assetPath.chain[0];

    return std::find(
        std::begin(BaseTransientPackageNames),
        std::end(BaseTransientPackageNames),
        StringHash(rootPackageName)) != std::end(BaseTransientPackageNames);
}

/*! \brief Is the AssetObject located in a package that allows us to move it elsewhere?
 *  Only Engine-defined internal packages (e.g $Memory, $Import, $Temp) enable this behaviour. */
static bool ShouldRelocateAssetBeforeSave(const AssetObject& assetObject)
{
    if (assetObject.GetAssetFlags() & AssetObjectFlags::TRANSIENT)
    {
        return false; // explicitly marked transient; don't move
    }

    return !assetObject.IsRegistered() || IsTransientAssetPath(assetObject.GetPath());
}

template <SizeType Size>
static bool IsPackageInList(
    const AssetPackage& package,
    const StringHash(&elems)[Size],
    bool exactMatch)
{
    StringHash substrHash = StringHash(package.GetName());

    if (!exactMatch)
    {
        const ANSIString packagePath = package.BuildPackagePath();
        const ANSIStringView substr = packagePath.Substr(0, packagePath.FindFirstIndex('/'));
        substrHash = StringHash(substr);
    }

    for (SizeType i = 0; i < Size; i++)
    {
        StringHash packageName = elems[i];

        if (substrHash == packageName)
        {
            return true;
        }
    }

    return false;
}

/*! \brief Check if the package should automatically save assets to disk when they are initially added
 *   to the package, rather than the standard protocol of marking the package dirty until save is invoked.
 *   
 *   This is to be used primarily for internal packages (e.g $Temp, Engine) */
static bool ShouldSavePackageOnChanged(const AssetPackage& package)
{
    if (package.IsTransient())
    {
        return false;
    }

    return IsPackageInList(package, PredefinedPackageNames, /* exactMatch */ false);
}

/*! \brief Check if we should rename assets that have names that are already used within the package.
 *  if returns true, asset `Foo` will be renamed to `Foo1` if there is already an asset named `Foo` in the package. */
static bool ShouldUniquifyAssetNames(const AssetPackage& package)
{
    // predefined packages
    const ANSIString packagePath = package.BuildPackagePath();
    const ANSIStringView substr = packagePath.Substr(0, packagePath.FindFirstIndex('/'));
    StringHash substrHash = substr;

    return substrHash != "Engine"_sh;
}

static TResult<Handle<AssetPackage>> RelocateAsset(
    AssetRegistry& registry,
    const Handle<AssetObject>& assetObject,
    UTF8StringView newPackageBasePath,
    bool preservePathStructure)
{
    Assert(assetObject.IsValid() && (!preservePathStructure || assetObject->IsRegistered()),
        "Invalid asset or invalid asset path. If preserveStructure is true, the asset must already have a path assigned");

    Array<Name> subpackageNames;

    Handle<AssetPackage> previousPackage = assetObject->GetPackage();

    if (previousPackage.IsValid())
    {
        // keep a copy around in case removing it from the package invalidates the reference
        Handle<AssetObject> assetObjectCopy = assetObject;

        // remove the asset from its current package
        if (Result removeResult = previousPackage->RemoveAssetObject(assetObject); removeResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to remove asset object '{}' from package '{}': {}", assetObject->GetName(), previousPackage->GetName(), removeResult.GetError().GetMessage());
        }
    }

    String newPath;

    if (preservePathStructure)
    {
        Handle<AssetPackage> currentPackage = previousPackage;

        while (currentPackage.IsValid() && !IsPackageInList(*currentPackage, BaseTransientPackageNames, /* exactMatch */ true))
        {
            subpackageNames.PushBack(currentPackage->GetName());
            currentPackage = currentPackage->GetParentPackage().Lock();
        }

        subpackageNames.Reverse();

        newPath = String(newPackageBasePath) + '/' + String::Join(subpackageNames, '/', &Name::LookupString);
    }
    else
    {
        newPath = newPackageBasePath;
    }

    HYP_LOG(Assets, Debug, "Relocating asset '{}' to: '{}'", assetObject->GetName(), newPath);

    if (Result registerAssetResult = registry.RegisterAsset(newPath, assetObject); registerAssetResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to relocate asset '{}' to '{}': {}", assetObject->GetName(), newPath, registerAssetResult.GetError().GetMessage());
    }

    Handle<AssetPackage> newPackage = assetObject->GetPackage();
    if (!newPackage.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Asset '{}' relocation did not assign a new package!", assetObject->GetName());
    }

    return newPackage;
}

#pragma region AssetPackage

AssetPackage::AssetPackage()
    : AssetPackage(Name::Invalid())
{
}

AssetPackage::AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags)
    : m_flags(flags),
      m_isLoading(false),
      m_isDirty(false)
{
    if (name.IsValid())
    {
        if (name == "$Memory"_sh || name == "$Import"_sh)
        {
            m_flags |= APF_HIDDEN | APF_TRANSIENT;
        }
        else
        {
            const char* str = name.LookupString();

            if (str[0] == '$')
            {
                m_flags |= APF_HIDDEN;
            }
        }

        m_name = SanitizeName(name);
        m_friendlyName = CreateFriendlyName(name);
    }
}

void AssetPackage::Init()
{
    HYP_SCOPE;

    ObjectBase::Init();

    Handle<AssetRegistry> registry = m_registry.Lock();
    Assert(registry.IsValid());

    Array<Handle<AssetObject>> assetObjects;
    Array<Handle<AssetPackage>> subpackages;

    HashSet<AssetObject*> assetObjectsToSave;

    bool isLoading = false;
    bool isPackageSavedInFilesystem = false;
    bool shouldSaveAssets = false;

    {
        TUniqueLock guard(m_mutex);

        isLoading = IsLoading();
        isPackageSavedInFilesystem = !IsTransient() && IsSaved_Internal();
        shouldSaveAssets = ShouldSavePackageOnChanged(*this) && !isLoading;

        assetObjects.Reserve(m_assetObjects.Size());
        subpackages.Reserve(m_subpackages.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            if (!shouldSaveAssets && !isPackageSavedInFilesystem && !isLoading)
            {
                assetObject->SetIsTransientByProxy(true);
            }

            if (shouldSaveAssets)
            {
                assetObjectsToSave.Insert(assetObject.Get());
            }

            InitObject(assetObject);

            assetObjects.PushBack(assetObject);
        }

        for (const Handle<AssetPackage>& subpackage : m_subpackages)
        {
            InitObject(subpackage);

            if (isPackageSavedInFilesystem && shouldSaveAssets)
            {
                FilePath subpackageDir = m_packageDir / *subpackage->GetName();

                Result savePackageResult = subpackage->Save(subpackageDir, /* saveEvenIfNotDirty */ true);
                if (savePackageResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save subpackage {} of {}: {}",
                        subpackage->GetName(), BuildPackagePath(), savePackageResult.GetError().GetMessage());
                }
            }

            OnSubpackageAdded(subpackage);
            subpackages.PushBack(subpackage);
        }
    }

    if (!shouldSaveAssets && !isLoading && assetObjects.Any())
    {
        MarkDirty(); // if not saving assets right now, need to mark it to be saved later
    }

    for (const Handle<AssetObject>& assetObject : assetObjects)
    {
        if (shouldSaveAssets && assetObjectsToSave.Contains(assetObject.Get()))
        {
            AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
                
            const FilePath newManifestFilepath = m_packageDir / *assetObject->GetName() + ".json";

            // save the asset in our package
            if (Result saveAssetResult = assetObject->Save(newManifestFilepath); saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
            }
            else
            {
                assetObject->SetIsTransientByProxy(false);
            }
        }

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }

    SetReady(true);
}

bool AssetPackage::IsSubpackageOf(const AssetPackage& other) const
{
    HYP_SCOPE;

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    while (parentPackage.IsValid())
    {
        if (parentPackage == &other)
        {
            return true;
        }

        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    return false;
}

void AssetPackage::SetAssets(const AssetObjectSet& assetObjects)
{
    HYP_SCOPE;

    AssetObjectSet previousAssetObjects;

    { // store so we can call OnAssetObjectRemoved outside of the lock
        TUniqueLock guard(m_mutex);

        previousAssetObjects = std::move(m_assetObjects);
    }

    for (const Handle<AssetObject>& assetObject : previousAssetObjects)
    {
        assetObject->m_package.Reset();

        OnAssetObjectRemoved(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectRemoved(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }

    previousAssetObjects.Clear();

    Array<Handle<AssetObject>> newAssetObjects;
    HashSet<AssetObject*> assetObjectsToSave;
    
    bool isLoading = false;
    bool shouldSaveAssets = false;
    bool isPackageSavedInFilesystem = false;

    {
        TUniqueLock guard(m_mutex);
        
        isLoading = IsLoading();
        isPackageSavedInFilesystem = !IsTransient() && IsSaved_Internal();
        shouldSaveAssets = ShouldSavePackageOnChanged(*this) && !isLoading;

        m_assetObjects = assetObjects;

        newAssetObjects.Reserve(m_assetObjects.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            AssertDebug(assetObject.IsValid());

            if (!assetObject.IsValid())
            {
                continue;
            }

            if (!assetObject->GetName().IsValid())
            {
                assetObject->m_name = GetUniqueAssetName_Internal(assetObject->InstanceClass()->GetName());
            }

            assetObject->m_package = WeakHandleFromThis();
            assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);
            
            AssertDebug(assetObject->m_assetPath.IsValid());

            if (shouldSaveAssets)
            {
                assetObjectsToSave.Insert(assetObject.Get());
            }
            else if (!shouldSaveAssets && !isPackageSavedInFilesystem)
            {
                assetObject->SetIsTransientByProxy(true);
            }

            InitObject(assetObject);

            newAssetObjects.PushBack(assetObject);
        }
    }

    if (!isLoading && !shouldSaveAssets)
    {
        MarkDirty();
    }

    for (const Handle<AssetObject>& assetObject : newAssetObjects)
    {
        if (isLoading)
        {
            assetObject->SetIsTransientByProxy(false);
        }
        else if (shouldSaveAssets && assetObjectsToSave.Contains(assetObject.Get()))
        {
            AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
                
            const FilePath newManifestFilepath = m_packageDir / *assetObject->GetName() + ".json";

            // save the file in our package
            Result saveAssetResult = assetObject->Save(newManifestFilepath);

            if (saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
            }
            else
            {
                assetObject->SetIsTransientByProxy(false);
            }
        }

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }
}

Result AssetPackage::AddAssetObject(const Handle<AssetObject>& assetObject)
{
    HYP_SCOPE;

    if (!assetObject.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    if (assetObject->m_package.GetUnsafe() == this)
    {
        // already added, fine
        return {};
    }

    if (assetObject->IsRegistered())
    {
        Handle<AssetPackage> currentPackage = assetObject->GetPackage();
        AssertDebug(currentPackage != nullptr);

        if (currentPackage)
        {
            HYP_LOG(Assets, Info, "AssetObject '{}' belongs to package {} and will be removed from it before being added to package {}",
                assetObject->GetName(),
                currentPackage->BuildPackagePath(),
                BuildPackagePath());

            if (Result result = currentPackage->RemoveAssetObject(assetObject); result.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to remove AssetObject {} from package {}! Error was: {}",
                    assetObject->GetName(),
                    currentPackage->BuildPackagePath(),
                    result.GetError().GetMessage());

                return result;
            }
        }
    }

    assetObject->m_package = WeakHandleFromThis();

    bool isLoading = false;
    bool isPackageSavedInFilesystem = false;
    bool shouldSaveAsset = false;

    {
        TUniqueLock guard(m_mutex);

        isLoading = IsLoading();
        isPackageSavedInFilesystem = !IsTransient() && IsSaved_Internal();
        shouldSaveAsset = ShouldSavePackageOnChanged(*this) && !isLoading;

        // if no name is provided for the asset, generate one
        if (!assetObject->m_name.IsValid())
        {
            assetObject->m_name = GetUniqueAssetName_Internal(assetObject->InstanceClass()->GetName());
        }
        
        assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

        if (!shouldSaveAsset && !isPackageSavedInFilesystem && !isLoading)
        {
            assetObject->SetIsTransientByProxy(true);
        }

        auto existingAssetObjectIt = m_assetObjects.Find(assetObject->GetName());

        if (existingAssetObjectIt != m_assetObjects.End())
        {
            if (*existingAssetObjectIt != assetObject)
            {
                return HYP_MAKE_ERROR(Error, "AssetObject with name '{}' already exists in package '{}'", assetObject->GetName(), BuildPackagePath());
            }

            // already exists and is the same object; fine
            return {};
        }
        
        m_assetObjects.Insert({ assetObject });
    }

    InitObject(assetObject);

    if (isLoading)
    {
        assetObject->SetIsTransientByProxy(false);
    }
    else if (!shouldSaveAsset)
    {
        MarkDirty();
    }

    HYP_LOG(Assets, Debug, "Added {} '{}' to package '{}'",
        assetObject->InstanceClass()->GetName(),
        assetObject->GetName(),
        BuildPackagePath());
    
    if (shouldSaveAsset)
    {
        AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
            
        const FilePath newManifestFilepath = m_packageDir / *assetObject->GetName() + ".json";

        // save the file in our package
        Result saveAssetResult = assetObject->Save(newManifestFilepath);

        if (saveAssetResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
        }
        else
        {
            assetObject->SetIsTransientByProxy(false);
        }
    }

    OnAssetObjectAdded(assetObject, true);

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    while (parentPackage.IsValid())
    {
        parentPackage->OnAssetObjectAdded(assetObject, false);
        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    return {};
}

Result AssetPackage::RemoveAssetObject(const Handle<AssetObject>& assetObject)
{
    HYP_SCOPE;

    if (!assetObject)
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    {
        TUniqueLock guard(m_mutex);

        auto it = m_assetObjects.Find(assetObject->GetName());

        if (it == m_assetObjects.End())
        {
            return HYP_MAKE_ERROR(Error, "AssetObject '{}' not found in package '{}'", assetObject->GetName(), m_name);
        }

        m_assetObjects.Erase(it);

        assetObject->m_package.Reset();
        assetObject->m_assetPath = {};
    }

    MarkDirty();

    OnAssetObjectRemoved(assetObject, true);

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    while (parentPackage.IsValid())
    {
        parentPackage->OnAssetObjectRemoved(assetObject, false);
        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    HYP_LOG(Assets, Debug, "Removed {} '{}' from package '{}'",
        assetObject->InstanceClass()->GetName(),
        assetObject->GetName(),
        BuildPackagePath());

    /// TODO: remove the file

    return {};
}

Handle<AssetObject> AssetPackage::GetAssetObject(StringHash assetName) const
{
    if (!assetName.IsValid())
    {
        return {};
    }

    TSharedLock guard(m_mutex);

    auto it = m_assetObjects.FindAs(assetName);

    if (it == m_assetObjects.End())
    {
        return {};
    }

    return *it;
}

Result AssetPackage::MergePackage(const Handle<AssetPackage>& package)
{
    HYP_SCOPE;

    if (!package.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Package is invalid");
    }

    if (package == this)
    {
        return HYP_MAKE_ERROR(Error, "Cannot merge package '{}' into itself", m_name);
    }

    HashSet<Name> currentAssetNames;
    ForEachAssetObject([&](const Handle<AssetObject>& asset)
        {
            currentAssetNames.Insert(asset->GetName());

            return IterationResult::CONTINUE;
        });

    Array<Handle<AssetObject>> assets;
    package->ForEachAssetObject([&](const Handle<AssetObject>& asset)
        {
            assets.PushBack(asset);

            return IterationResult::CONTINUE;
        });

    // Remove assets from the package and add them to the new package - renaming if necessary to avoid name clashes
    for (const Handle<AssetObject>& asset : assets)
    {
        if (!asset.IsValid())
        {
            continue;
        }

        Name desiredName = asset->GetName();

        const bool renameOnNameClash = ShouldUniquifyAssetNames(*this);

        // check if name is already taken in destination package
        if (renameOnNameClash && currentAssetNames.Contains(desiredName))
        {
            Name uniqueName = GetUniqueAssetName(desiredName);

            if (Result renameResult = asset->Rename(uniqueName); renameResult.HasError())
            {
                HYP_LOG(Assets, Warning, "Failed to rename asset '{}' during merge: {}", desiredName, renameResult.GetError().GetMessage());

                continue;
            }
        }

        if (Result removeResult = package->RemoveAssetObject(asset); removeResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset '{}' from source package '{}' during merge: {}", asset->GetName(), package->GetName(), removeResult.GetError().GetMessage());

            continue;
        }

        if (!renameOnNameClash)
        {
            if (currentAssetNames.Contains(desiredName))
            {
                Handle<AssetObject> existingAssetObject = GetAssetObject(desiredName);

                // only try to remove if it actually exists and is valid, otherwise we don't care
                if (existingAssetObject.IsValid())
                {
                    // remove old asset and overwrite it
                    Result removeResult = RemoveAssetObject(existingAssetObject);

                    if (removeResult.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to remove clashing asset with name '{}' from package: '{}' during merge: {}",
                            asset->GetName(), GetName(), removeResult.GetError().GetMessage());

                        continue; // skip adding
                    }
                }
            }
        }

        if (Result addResult = AddAssetObject(asset); addResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to add asset '{}' to destination package '{}' during merge: {}", asset->GetName(), GetName(), addResult.GetError().GetMessage());
        }
    }

    Handle<AssetPackage> strongThis = MakeStrongRef(this);

    // needed for GetPackageFromPath() / GetSubpackage().
    /// \todo : Refactor to call these methods on AssetPackage directly?
    Handle<AssetRegistry> registry = m_registry.Lock();
    Assert(registry != nullptr);

    Optional<Error> mergeError;

    package->ForEachSubpackage([&](const Handle<AssetPackage>& sub)
        {
            if (!sub)
            {
                return IterationResult::CONTINUE;
            }

            Handle<AssetPackage> dest = registry->GetSubpackage(strongThis, sub->GetName(), /* createIfNotExist */ true);
            Assert(dest != nullptr);

            if (Result mergeResult = dest->MergePackage(sub); mergeResult.HasError())
            {
                mergeError = mergeResult.GetError();

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        });

    if (mergeError.HasValue())
    {
        return *mergeError;
    }

    return {};
}

String AssetPackage::BuildPackagePath() const
{
    HYP_SCOPE;

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    if (!parentPackage.IsValid())
    {
        return *m_name;
    }

    return parentPackage->BuildPackagePath() + "/" + *m_name;
}

AssetPath AssetPackage::BuildAssetPath(Name assetName) const
{
    HYP_SCOPE;

    if (!assetName.IsValid())
    {
        HYP_BREAKPOINT;
        return {};
    }

    Array<Name> chain;

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    while (parentPackage.IsValid())
    {
        chain.PushBack(parentPackage->m_name);

        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    chain.Reverse();
    chain.PushBack(m_name);
    chain.PushBack(assetName);

    AssetPath assetPath;
    assetPath.SetChain(chain);

    AssertDebug(assetPath.IsValid());

    return assetPath;
}

void AssetPackage::Rename(Name name)
{
    HYP_SCOPE;

    if (m_name == name || !name.IsValid())
    {
        return;
    }

    if (name == "$Memory"_sh || name == "$Import"_sh)
    {
        m_flags |= APF_HIDDEN | APF_TRANSIENT;
    }
    else
    {
        const char* str = name.LookupString();

        if (str[0] == '$')
        {
            m_flags |= APF_HIDDEN;
        }
    }

    name = SanitizeName(name);
    Name friendlyName = CreateFriendlyName(name);

    TSharedLock thisPackageSharedLock(m_mutex);

    const Name oldName = m_name;

    Handle<AssetPackage> strongThis = MakeStrongRef(this);

    Handle<AssetRegistry> registry = m_registry.Lock();
    AssertDebug(registry.IsValid());

    auto UpdateAssetPaths = [this]()
    {
        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            AssertDebug(assetObject.IsValid());

            if (!assetObject.IsValid())
            {
                continue;
            }

            AssertDebug(assetObject->m_package.GetUnsafe() == this);

            assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);
        }
    };

    // make sure we have a unique asset name within parent package
    if (Handle<AssetPackage> parentPackage = m_parentPackage.Lock(); parentPackage.IsValid())
    {
        { // remove the package first. need to do this since we hash by name.
            TUniqueLock parentPackageLock(parentPackage->m_mutex);
            parentPackage->m_subpackages.Erase(oldName);
        }

        parentPackage->OnSubpackageRemoved(strongThis);
        registry->OnPackageRemoved(strongThis);

        { // re-lock
            thisPackageSharedLock.Reset();
            m_mutex.LockWriter();

            TUniqueLock parentPackageLock(parentPackage->m_mutex);
            m_name = GetUniqueName(name, parentPackage->m_subpackages);

            m_friendlyName = friendlyName;

            UpdateAssetPaths();

            m_mutex.UnlockWriter();

            parentPackage->m_subpackages.Add(strongThis);
        }

        parentPackage->OnSubpackageAdded(strongThis);
        registry->OnPackageAdded(strongThis);
    }
    else // top-level package
    {
        {
            TUniqueLock registryLock(registry->m_mutex);
            registry->m_packages.Erase(oldName);
        }

        registry->OnPackageRemoved(strongThis);

        {
            thisPackageSharedLock.Reset();
            m_mutex.LockWriter();

            TUniqueLock registryLock(registry->m_mutex);
            m_name = GetUniqueName(name, registry->m_packages);

            m_friendlyName = friendlyName;

            UpdateAssetPaths();

            m_mutex.UnlockWriter();

            registry->m_packages.Add(strongThis);
        }

        registry->OnPackageAdded(strongThis);
    }
}

bool AssetPackage::HasAssetWithName(Name assetName) const
{
    HYP_SCOPE;

    if (!assetName.IsValid())
    {
        return false;
    }

    TSharedLock guard(m_mutex);
    return m_assetObjects.Contains(assetName);
}

Name AssetPackage::GetUniqueAssetName(Name baseName) const
{
    HYP_SCOPE;

    if (!baseName.IsValid())
    {
        return Name::Invalid();
    }

    TSharedLock guard(m_mutex);

    return GetUniqueAssetName_Internal(baseName);
}

Name AssetPackage::GetUniqueAssetName_Internal(Name baseName) const
{
    HYP_SCOPE;

    return GetUniqueName(baseName, m_assetObjects);
}

Name AssetPackage::GetUniqueSubpackageName_Internal(Name baseName) const
{
    HYP_SCOPE;

    return GetUniqueName(baseName, m_subpackages);
}

Result AssetPackage::Save(const FilePath& outputDirectory, bool saveEvenIfNotDirty)
{
    HYP_SCOPE;

    TUniqueLock guard(m_mutex);

    if (IsTransient())
    {
        return HYP_MAKE_ERROR(Error, "Cannot save transient AssetPackage '{}'", m_name);
    }

    bool skipSavingThisPackage = false;

    // If saveEvenIfNotDirty is false (default), check if we should save
    //  - if it has been saved before, we need to check if is dirty
    //    and additionally check if any individual asset objects are dirty.
    if (!saveEvenIfNotDirty && IsSaved_Internal())
    {
        if (!IsDirty())
        {
            if (HasDirtyAssetObjects())
            {
                MarkDirty();
            }
        }

        if (!IsDirty())
        {
            // Already saved and not marked dirty; return ok
            skipSavingThisPackage = true;
        }
    }

    Handle<AssetRegistry> registry = m_registry.Lock();

    if (!registry)
    {
        return HYP_MAKE_ERROR(Error, "AssetPackage '{}' does not have a valid AssetRegistry", m_name);
    }

    FilePath packageDir;

    // Build package save dir. `outputDirectory` /may/ just be a base path, where we'll append the package path to it
    const String packagePath = BuildPackagePath();

    Array<String> outputParts = outputDirectory.Split('/', '\\');
    Array<String> packageParts = packagePath.Split('/', '\\');

    SizeType packageStartIndex = 0;

    if (packageParts.Any() && outputParts.Any())
    {
        // Check if the last part of outputDirectory matches any part of packagePath
        const String& lastOutputPart = outputParts.Back();

        for (SizeType i = 0; i < packageParts.Size(); ++i)
        {
            if (packageParts[i] == lastOutputPart)
            {
                packageStartIndex = i + 1; // Start after the matched component
                break;
            }
        }
    }

    if (packageStartIndex < packageParts.Size())
    {
        // Append remaining package path components
        Array<String> remainingParts;
        for (SizeType i = packageStartIndex; i < packageParts.Size(); ++i)
        {
            remainingParts.PushBack(packageParts[i]);
        }

        if (remainingParts.Any())
        {
            packageDir = outputDirectory / String::Join(remainingParts, '/');
        }
        else
        {
            packageDir = outputDirectory;
        }
    }
    else
    {
        // All package components already exist in output path
        packageDir = outputDirectory;
    }

    if (!packageDir.Exists())
    {
        if (!packageDir.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create package directory '{}'", outputDirectory);
        }

        // Path we have didn't exist on the file system, so treat it as a new save.
        saveEvenIfNotDirty = true;
        skipSavingThisPackage = false;
    }
    else if (!packageDir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' already exists and is not a directory", outputDirectory);
    }

    // If package dir is different than what the package is saved with,
    // we need to save again. This will be used when performing "Save As", for example
    if (IsSaved_Internal() && m_packageDir != packageDir)
    {
        saveEvenIfNotDirty = true;
        skipSavingThisPackage = false;
    }

    if (!skipSavingThisPackage)
    {
        const FilePath manifestPath = packageDir / "PackageManifest.json";

        FileByteWriter manifestWriter { manifestPath };

        if (!manifestWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open manifest file for package '{}', errno: {}", m_name, std::strerror(errno));
        }

        if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to save manifest for package '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
        }

        manifestWriter.Close();

        m_packageDir = packageDir;
    }

    // even if skipSaving is true, we need to iterate over subpackages as
    // they may have individual asset objects that are dirty
    for (const Handle<AssetPackage>& subpackage : m_subpackages)
    {
        if (subpackage->IsTransient())
        {
            continue;
        }

        Result result = subpackage->Save(m_packageDir / *subpackage->GetName(), saveEvenIfNotDirty);

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save subpackage '{}' of package '{}': {}", subpackage->GetName(), m_name, result.GetError().GetMessage());
        }
    }

    if (!skipSavingThisPackage && !IsTransient() && IsSaved_Internal())
    {
        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            // If TRANSIENT (not BY PROXY), skip saving this asset
            if ((assetObject->GetAssetFlags() & (AssetObjectFlags::TRANSIENT | AssetObjectFlags::TRANSIENT_BY_PROXY)) == AssetObjectFlags::TRANSIENT)
            {
                continue;
            }

            if (Result saveAssetResult = assetObject->Save(m_packageDir / *assetObject->GetName() + ".json"); saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
                continue;
            }

            assetObject->SetIsTransientByProxy(false);
        }

        // unset dirty state
        m_isDirty.Set(false, MemoryOrder::RELEASE);
    }

    return {};
}

Result AssetPackage::SaveManifest(ByteWriter& stream) const
{
    HYP_SCOPE;

    JSON::JSObject manifestJson;
    ObjectToJSON(InstanceClass(), BoxedValue(HandleFromThis()), manifestJson);

    // need to set virtual path property for loading
    manifestJson["Path"] = *BuildPackagePath();

    stream.WriteString(JSON::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

bool AssetPackage::HasDirtyAssetObjects() const
{
    // assume mtx is locked

    return m_assetObjects.FindIf([](AssetObject* obj)
               {
                   return obj->IsDirty();
               })
        != m_assetObjects.End();
}

void AssetPackage::AddDependency(const AssetPath& dependency)
{
    HYP_SCOPE;

    {
        if (!dependency.chain || !dependency.chain[0].IsValid() || dependency == AssetPath(BuildPackagePath()))
        {
            return;
        }

        // Check for circular dependency
        Handle<AssetRegistry> registry = m_registry.Lock();

        if (registry.IsValid())
        {
            Handle<AssetPackage> dependencyPackage = registry->GetPackageFromPath(dependency.ToString(), /* createIfNotExist */ false);

            if (dependencyPackage)
            {
                if (dependencyPackage->IsTransient())
                {
                    HYP_LOG(Assets, Warning, "Adding dependency on transient package '{}'; may cause loading to fail or other errors.", dependencyPackage->BuildPackagePath());
                }

                // Check if the dependency package depends on us (circular dependency)
                const AssetPath thisPath = AssetPath(BuildPackagePath());

                for (const AssetPath& subDependency : dependencyPackage->m_dependencies)
                {
                    if (subDependency == thisPath)
                    {
                        HYP_LOG(Assets, Warning, "Circular dependency detected: Package '{}' and '{}' depend on each other. Skipping dependency.", thisPath.ToString(), dependency.ToString());
                        return;
                    }
                }
            }
        }

        TUniqueLock guard(m_mutex);

        if (!m_dependencies.Contains(dependency))
        {
            m_dependencies.PushBack(dependency);

            HYP_LOG(Assets, Debug, "Added dependency to package '{}': {}", m_name, dependency.ToString());
        }
    }

    if (!IsLoading())
        MarkDirty();
}

Array<String> AssetPackage::GetRelativeDependencies() const
{
    HYP_SCOPE;
    Array<String> result;

    const AssetPath thisPackagePath = AssetPath(BuildPackagePath());

    for (const AssetPath& dependency : m_dependencies)
    {
        result.PushBack(AssetPath::MakeRelativePath(thisPackagePath, dependency));
    }

    return result;
}

void AssetPackage::SetRelativeDependencies(const Array<String>& relativePaths)
{
    HYP_SCOPE;

    {
        TUniqueLock guard(m_mutex);

        m_dependencies.Clear();

        const AssetPath thisPackagePath = AssetPath(BuildPackagePath());

        Handle<AssetRegistry> registry = m_registry.Lock();

        for (const String& path : relativePaths)
        {
            const AssetPath dependencyPath = AssetPath::FromRelativePath(thisPackagePath, path);

            if (dependencyPath.chain && dependencyPath.chain[0] != Name::Invalid() && dependencyPath != thisPackagePath)
            {
                // Check for circular dependency
                bool isCircular = false;

                if (registry.IsValid())
                {
                    Handle<AssetPackage> dependencyPackage = registry->GetPackageFromPath(dependencyPath.ToString(), /* createIfNotExist */ false);

                    if (dependencyPackage.IsValid())
                    {
                        // Check if the dependency package depends on us (circular dependency)
                        for (const AssetPath& depOfDep : dependencyPackage->m_dependencies)
                        {
                            if (depOfDep == thisPackagePath)
                            {
                                HYP_LOG(Assets, Warning, "Circular dependency detected: Package '{}' and '{}' depend on each other. Skipping dependency.", thisPackagePath.ToString(), dependencyPath.ToString());
                                isCircular = true;
                                break;
                            }
                        }
                    }
                }

                if (!isCircular)
                {
                    m_dependencies.PushBack(dependencyPath);
                }
            }
        }
    }

    if (!IsLoading())
        MarkDirty();
}

void AssetPackage::Prune(Array<Handle<AssetPackage>>& outRemovedPackages, bool* outShouldDestroy)
{
    HYP_SCOPE;

    // Prune no longer ref'd asset objects
    if (IsTransient())
    {
        Array<Handle<AssetObject>> objectsToDelete;

        {
            TUniqueLock guard(m_mutex);

            for (auto it = m_assetObjects.Begin(); it != m_assetObjects.End();)
            {
                const Handle<AssetObject>& assetObject = *it;

                // @FIXME Will have issues with .NET objects having extra ref attached, needs to account for that
                if (assetObject->GetObjectHeader_Internal()->GetRefCountStrong() == 1) // only referenced by us
                {
                    objectsToDelete.PushBack(assetObject);

                    HYP_LOG(Assets, Debug, "Pruning asset object '{}' from package '{}'", assetObject->GetName(), m_name);

                    assetObject->SetIsTransientByProxy(false);
                    assetObject->m_package.Reset();
                    assetObject->m_assetPath = {};

                    it = m_assetObjects.Erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        if (objectsToDelete.Any())
        {
            MarkDirty();

            for (const Handle<AssetObject>& assetObject : objectsToDelete)
            {
                OnAssetObjectRemoved(assetObject, true);

                Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

                while (parentPackage.IsValid())
                {
                    parentPackage->OnAssetObjectRemoved(assetObject, false);
                    parentPackage = parentPackage->GetParentPackage().Lock();
                }
            }
        }
    }

    // Prune subpackages
    Array<Handle<AssetPackage>> subpackagesToDelete;

    {
        Array<Handle<AssetPackage>> subpackagesToPrune;

        {
            TSharedLock guard(m_mutex);

            for (const Handle<AssetPackage>& subpackage : m_subpackages)
            {
                if (!subpackage)
                {
                    continue;
                }

                subpackagesToPrune.PushBack(subpackage);
            }
        }

        for (const Handle<AssetPackage>& subpackage : subpackagesToPrune)
        {
            subpackage->Prune(outRemovedPackages);
        }

        {
            TUniqueLock guard(m_mutex);

            for (auto it = m_subpackages.Begin(); it != m_subpackages.End();)
            {
                const Handle<AssetPackage>& subpackage = *it;

                if (!subpackage)
                {
                    ++it;
                    continue;
                }

                TUniqueLock guard2(subpackage->m_mutex);
                if (subpackage->m_assetObjects.Empty() && subpackage->m_subpackages.Empty())
                {
                    subpackagesToDelete.PushBack(subpackage);

                    it = m_subpackages.Erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // Remove empty subpackages

    if (subpackagesToDelete.Any())
    {
        MarkDirty();

        for (Handle<AssetPackage>& subpackage : subpackagesToDelete)
        {
            OnSubpackageRemoved(subpackage);

            Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

            while (parentPackage.IsValid())
            {
                parentPackage->OnSubpackageRemoved(subpackage);

                parentPackage = parentPackage->GetParentPackage().Lock();
            }

            outRemovedPackages.PushBack(std::move(subpackage));
        }
    }

    if (outShouldDestroy != nullptr)
    {
        TSharedLock guard(m_mutex);
        *outShouldDestroy = m_assetObjects.Empty() && m_subpackages.Empty();
    }
}

void AssetPackage::MarkDirty()
{
    HYP_SCOPE;

    constexpr int MaxRecursionDepth = 32;
    static thread_local int s_recursionDepth = 0;

    HYP_DEFER({ --s_recursionDepth; });

    if (s_recursionDepth++ > MaxRecursionDepth)
    {
        HYP_LOG(Assets, Error, "Max recursion depth reached in AssetPackage::MarkDirty for package '{}'", m_name);

        return;
    }

    if (!m_isDirty.Exchange(true, MemoryOrder::ACQUIRE_RELEASE))
    {
        TSharedLock guard(m_mutex);

        if (Handle<AssetPackage> parentPackage = m_parentPackage.Lock(); parentPackage.IsValid())
        {
            guard.Reset();

            parentPackage->MarkDirty();
        }
    }
}

#pragma endregion AssetPackage

#pragma region AssetRegistry

AssetRegistry::AssetRegistry()
    : AssetRegistry("res")
{
}

AssetRegistry::AssetRegistry(const String& rootPath)
    : m_rootPath(rootPath),
      m_scheduler(new Scheduler(s_assetRegistryThread)),
      m_pruneTimer { 30.0 }, // every 30 seconds
      m_pruneTaskBatch(nullptr)
{
}

AssetRegistry::~AssetRegistry()
{
    if (m_pruneTaskBatch != nullptr)
    {
        if (!m_pruneTaskBatch->IsCompleted())
        {
            HYP_LOG(Assets, Warning, "Waiting for prune task batch to complete before destroying AssetRegistry...");
            m_pruneTaskBatch->AwaitCompletion();
        }

        delete m_pruneTaskBatch;
        m_pruneTaskBatch = nullptr;
    }

    delete m_scheduler;
}

void AssetRegistry::Initialize()
{
    HYP_SCOPE;

    LoadPackagesAsync(/* loadSubpackages */ false);

    Handle<AssetPackage> enginePackage = GetPackageFromPath("Engine", true);
    enginePackage->Save(g_assetManager->GetBasePath());

    Handle<AssetPackage> tempPackage = GetPackageFromPath("$Temp", true);
    tempPackage->Save(CoreApi::GetExecutablePath());

    Handle<AssetPackage> memoryPackage = GetPackageFromPath("$Memory", true);

#ifdef HYP_EDITOR
    // Add transient package for imported assets in editor mode
    Handle<AssetPackage> importsPackage = GetPackageFromPath("$Import", true);
#endif
}

void AssetRegistry::PruneTransientPackages()
{
    if (m_pruneTaskBatch != nullptr)
    {
        // wait for previous prune to finish before starting a new one
        if (!m_pruneTaskBatch->IsCompleted())
        {
            HYP_LOG(Assets, Warning, "Previous prune task batch is still running, is it stuck?");
            return;
        }
    }

    if (!m_pruneTaskBatch)
    {
        TaskThreadPool* assetWorkerThreadPool = g_assetManager->GetThreadPool();
        AssertDebug(assetWorkerThreadPool != nullptr);

        m_pruneTaskBatch = new TaskBatch;
        m_pruneTaskBatch->pool = assetWorkerThreadPool;
    }
    else
    {
        m_pruneTaskBatch->ResetState();
    }

    { // collect tasks
        TSharedLock guard(m_mutex);
        for (const Handle<AssetPackage>& package : m_packages)
        {
            if (!package || !package->IsTransient())
            {
                continue;
            }

            m_pruneTaskBatch->AddTask([weakThis = MakeWeakRef(this), packageWeak = MakeWeakRef(package)]()
                {
                    constexpr bool ShouldRemoveEmptyRootPackages = false;

                    Handle<AssetPackage> package = packageWeak.Lock();
                    if (!package)
                    {
                        return;
                    }

                    Array<Handle<AssetPackage>> removedSubpackages;

                    bool shouldDestroy = false;
                    package->Prune(removedSubpackages, &shouldDestroy);
                    
                    Handle<AssetRegistry> registry = weakThis.Lock();
                    AssertDebug(registry.IsValid());

                    if (!registry.IsValid())
                    {
                        return;
                    }

                    // we broadcast OnPackageRemoved() even when subpackages are removed
                    if (removedSubpackages.Any())
                    {
                        for (Handle<AssetPackage>& subpackage : removedSubpackages)
                        {
                            registry->OnPackageRemoved(subpackage);
                        }
                    }

                    if (ShouldRemoveEmptyRootPackages && shouldDestroy)
                    {
                        // Remove the now empty package from the registry
                        registry->RemovePackage(package);
                    }
                });
        }
    }

    if (m_pruneTaskBatch->executors.Size() > 0)
    {
        TaskSystem::GetInstance().EnqueueBatch(m_pruneTaskBatch);
    }
}

void AssetRegistry::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

    if (!m_pruneTimer.Waiting())
    {
        m_pruneTimer.NextTick();

        PruneTransientPackages();
    }

    if (m_scheduler->NumEnqueued() > 0)
    {
        Queue<Scheduler::ScheduledTask> tasks;
        m_scheduler->AcceptAll(tasks);

        while (tasks.Any())
        {
            Scheduler::ScheduledTask scheduledTask = tasks.Pop();

            scheduledTask.Execute();
        }
    }
}

template <class Func, class FutureType>
void AssetRegistry::PostTask(Func&& fn, Task<FutureType>* pOutFuture)
{
    HYP_SCOPE;

    if (!UseSingleThread || IsOnThread(s_assetRegistryThread))
    {
        if (pOutFuture)
        {
            *pOutFuture = Task<FutureType>();

            if constexpr (std::is_void_v<FutureType>)
            {
                fn();
                pOutFuture->Fulfill();
            }
            else
            {
                pOutFuture->Fulfill(fn());
            }
        }
        else
        {
            fn();
        }

        return;
    }

    if (pOutFuture)
    {
        *pOutFuture = Task<FutureType>();

        m_scheduler->Enqueue([promise = pOutFuture->Promise(), fn = std::forward<Func>(fn)]() mutable
            {
                if constexpr (std::is_void_v<FutureType>)
                {
                    fn();
                    promise->Fulfill();
                }
                else
                {
                    promise->Fulfill(fn());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
    else
    {
        m_scheduler->Enqueue(std::forward<Func>(fn), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

/// \todo Revisit, this will have more overhead now that we execute a lot of functions
// on the sim thread (would probably be faster to just do it synchronously)
void AssetRegistry::LoadPackagesAsync(bool loadSubpackages)
{
    HYP_SCOPE;

    FilePath rootDir = g_assetManager->GetBasePath();

    if (!rootDir.Exists() || !rootDir.IsDirectory())
    {
        // nothing to load if it doesnt exist
        return;
    }

    TaskSystem::GetInstance().Enqueue([this, weakThis = WeakHandleFromThis(), rootDir, loadSubpackages]()
        {
            HYP_NAMED_SCOPE("AssetRegistry::LoadPackagesAsync");

            Handle<AssetRegistry> registry = weakThis.Lock();
            if (!registry)
            {
                HYP_LOG(Assets, Error, "AssetRegistry is no longer valid, cannot load packages");
                return;
            }

            AssetPackageSet rootPackages;

            Proc<void(const FilePath& dir)> iterateDirectory;

            iterateDirectory = [&](const FilePath& dir)
            {
                bool packageFound = false;

                const FilePath manifestPath = dir / "PackageManifest.json";

                if (manifestPath.Exists() && !manifestPath.IsDirectory())
                {
                    TResult<Handle<AssetPackage>> subpackageResult = LoadPackageFromManifest(manifestPath, loadSubpackages, /* forceLoad */ false);

                    // build virtual package path from filesystem path
                    if (subpackageResult.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load package from manifest '{}': {}", manifestPath, subpackageResult.GetError().GetMessage());

                        return;
                    }

                    Handle<AssetPackage> package = std::move(*subpackageResult);

                    if (!package.IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' is invalid!", dir);

                        return;
                    }

                    if (!package->GetName().IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' has an invalid name!", dir);

                        return;
                    }

                    rootPackages.Insert(package);

                    // if package manifest found, stop searching in directory and don't look deeper (LoadPackageFromManifest already handles subdirs)
                    return;
                }

                for (const FilePath& subdirectory : dir.GetSubdirectories())
                {
                    // recursively iterate subdirectories
                    iterateDirectory(subdirectory);
                }
            };

            iterateDirectory(rootDir);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND, TaskEnqueueFlags::FIRE_AND_FORGET);
}

void AssetRegistry::SetRootPath(const String& rootPath)
{
    HYP_SCOPE;

    TUniqueLock guard(m_mutex);

    m_rootPath = rootPath;
}

void AssetRegistry::SetPackages(const AssetPackageSet& packages)
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

    Proc<void(Handle<AssetPackage>)> initializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    initializePackage = [this, &initializePackage](const Handle<AssetPackage>& package)
    {
        Assert(package.IsValid());

        package->m_registry = MakeWeakRef(this);

        InitObject(package);

        OnPackageAdded(package);

        for (const Handle<AssetPackage>& subpackage : package->m_subpackages)
        {
            subpackage->m_parentPackage = package;
            subpackage->m_flags |= package->m_flags;

            initializePackage(subpackage);
        }
    };

    {
        TUniqueLock guard(m_mutex);

        for (const Handle<AssetPackage>& package : packages)
        {
            Assert(package.IsValid());

            m_packages.Set(package);
        }
    }

    for (const Handle<AssetPackage>& package : packages)
    {
        initializePackage(package);
    }
}

Result AssetRegistry::AddPackage(const Handle<AssetPackage>& package, bool mergeIfExists)
{
    HYP_SCOPE;

    if (!package.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Package is invalid");
    }

    const String packagePath = package->BuildPackagePath();
    Handle<AssetPackage> existing = GetPackageFromPath(packagePath, /* createIfNotExist */ false);

    if (existing.IsValid())
    {
        if (existing == package)
        {
            // already added, return early
            return {};
        }

        if (!mergeIfExists)
        {
            return HYP_MAKE_ERROR(Error, "Package with path '{}' already exists", packagePath);
        }

        /// TODO: Refactor to use `MergePackage` when AssetRegistry is not needed for `GetSubpackage()`
        Proc<void(const Handle<AssetPackage>&, const Handle<AssetPackage>&)> mergeInto;

        mergeInto = [this, &mergeInto](const Handle<AssetPackage>& dest, const Handle<AssetPackage>& src)
        {
            if (!dest.IsValid() || !src.IsValid())
            {
                return;
            }

            HashSet<Name> destAssetNames;
            dest->ForEachAssetObject([&](const Handle<AssetObject>& asset)
                {
                    destAssetNames.Insert(asset->GetName());

                    return IterationResult::CONTINUE;
                });

            // Move asset objects
            Array<Handle<AssetObject>> assets;
            src->ForEachAssetObject([&](const Handle<AssetObject>& asset)
                {
                    assets.PushBack(asset);

                    return IterationResult::CONTINUE;
                });

            for (const Handle<AssetObject>& asset : assets)
            {
                if (!asset.IsValid())
                {
                    continue;
                }

                Name desiredName = asset->GetName();

                const bool renameOnNameClash = ShouldUniquifyAssetNames(*dest);

                // check if name is already taken in destination package
                if (renameOnNameClash && destAssetNames.Contains(desiredName))
                {
                    Name uniqueName = dest->GetUniqueAssetName(desiredName);

                    if (Result renameResult = asset->Rename(uniqueName); renameResult.HasError())
                    {
                        HYP_LOG(Assets, Warning, "Failed to rename asset '{}' during merge: {}", desiredName, renameResult.GetError().GetMessage());

                        continue;
                    }
                }

                if (Result removeResult = src->RemoveAssetObject(asset); removeResult.HasError())
                {
                    HYP_LOG(Assets, Warning, "Failed to remove asset '{}' from source package '{}' during merge: {}", asset->GetName(), src->GetName(), removeResult.GetError().GetMessage());

                    continue;
                }

                if (!renameOnNameClash)
                {
                    if (destAssetNames.Contains(desiredName))
                    {
                        Handle<AssetObject> existingAssetObject = dest->GetAssetObject(desiredName);

                        // only try to remove if it actually exists and is valid, otherwise we don't care
                        if (existingAssetObject.IsValid())
                        {
                            // remove old asset and overwrite it
                            Result removeResult = dest->RemoveAssetObject(existingAssetObject);

                            if (removeResult.HasError())
                            {
                                HYP_LOG(Assets, Error, "Failed to remove clashing asset with name '{}' from package: '{}' during merge: {}",
                                    asset->GetName(), dest->GetName(), removeResult.GetError().GetMessage());

                                continue; // skip adding
                            }
                        }
                    }
                }

                // Add to destination
                Result addResult = dest->AddAssetObject(asset);
                if (addResult.HasError())
                {
                    HYP_LOG(Assets, Warning, "Failed to add asset '{}' to destination package '{}' during merge: {}", asset->GetName(), dest->GetName(), addResult.GetError().GetMessage());
                }
            }

            Array<Handle<AssetPackage>> subpackages;
            src->ForEachSubpackage([&](const Handle<AssetPackage>& sub)
                {
                    subpackages.PushBack(sub);

                    return IterationResult::CONTINUE;
                });

            for (const Handle<AssetPackage>& sub : subpackages)
            {
                if (!sub)
                {
                    continue;
                }

                Handle<AssetPackage> destSubpackage = GetSubpackage(dest, sub->GetName(), /* createIfNotExist */ true);
                Assert(destSubpackage != nullptr);

                mergeInto(destSubpackage, sub);
            }
        };

        mergeInto(existing, package);

        // update reference
        // package = existing;

        return {};
    }

    Handle<AssetPackage> newParentPackage;
    if (Handle<AssetPackage> prevParentPackage = package->GetParentPackage().Lock(); prevParentPackage != nullptr)
    {
        const String parentPackagePath = prevParentPackage->BuildPackagePath();

        newParentPackage = GetPackageFromPath(parentPackagePath, /* createIfNotExist */ true);
        Assert(newParentPackage != nullptr);
    }

    // to call OnPackageAdded with
    Array<Handle<AssetPackage>> addedPackages;

    Proc<void(Handle<AssetPackage>)> InitializePackage;
    InitializePackage = [this, &InitializePackage, &addedPackages](const Handle<AssetPackage>& pkg)
    {
        Assert(pkg != nullptr);

        pkg->m_registry = MakeWeakRef(this);

        addedPackages.PushBack(pkg);

        for (const Handle<AssetPackage>& sub : pkg->m_subpackages)
        {
            sub->m_parentPackage = pkg;
            sub->m_flags |= pkg->m_flags;

            InitializePackage(sub);
        }
    };

    InitializePackage(package);

    if (newParentPackage != nullptr)
    {
        bool isSubpackageSaved = false;

        {
            TUniqueLock guard(newParentPackage->m_mutex);

            package->m_parentPackage = newParentPackage;
            package->m_flags |= newParentPackage->m_flags;

            // If parent package exists on disk, save this package:
            if (!newParentPackage->IsTransient() && newParentPackage->IsSaved_Internal())
            {
                FilePath subpackageDir = newParentPackage->m_packageDir / *package->GetName();

                if (ShouldSavePackageOnChanged(*newParentPackage))
                {
                    Result savePackageResult = package->Save(subpackageDir, /* saveEvenIfNotDirty*/ true);
                    if (!savePackageResult.HasError())
                    {
                        isSubpackageSaved = true;
                    }
                    else
                    {
                        HYP_LOG(Assets, Error, "Failed to save subpackage {} of {}: {}",
                            package->GetName(), newParentPackage->BuildPackagePath(), savePackageResult.GetError().GetMessage());
                    }
                }
            }

            newParentPackage->m_subpackages.Insert(package);
            newParentPackage->OnSubpackageAdded(package);
        }

        if (!isSubpackageSaved)
        {
            // mark dirty on add if not saved via ShouldSavePackageOnChanged (recursively)
            package->MarkDirty();
        }
    }
    else // top-level package
    {
        TUniqueLock guard(m_mutex);

        m_packages.Insert(package);
    }

    for (const Handle<AssetPackage>& pkg : addedPackages)
    {
        InitObject(pkg);

        OnPackageAdded(pkg);
    }

    return {};
}

void AssetRegistry::RemovePackage(AssetPackage* package)
{
    HYP_SCOPE;

    if (!package)
    {
        HYP_LOG(Assets, Warning, "Cannot remove null package from AssetRegistry");
        return;
    }

    if (package->m_registry.GetUnsafe() != this)
    {
        HYP_LOG(Assets, Warning, "Cannot remove package '{}' from AssetRegistry it does not belong to", package->GetName());
        return;
    }

    package->m_registry.Reset();

    Handle<AssetPackage> strongPackage = MakeStrongRef(package);

    bool removed = false;

    {
        TUniqueLock lock(package->m_mutex);

        Handle<AssetPackage> parentPackage;

        if (package->m_parentPackage.IsValid())
        {
            parentPackage = package->m_parentPackage.Lock();

            if (parentPackage.IsValid())
            {
                auto it = parentPackage->m_subpackages.Find(package->GetName());
                Assert(it != parentPackage->m_subpackages.End());

                parentPackage->m_subpackages.Erase(it);
                
                lock.Reset(parentPackage->m_mutex);

                parentPackage->OnSubpackageRemoved(strongPackage);

                removed = true;

                // exit lock to mark dirty or else we'll deadlock:
                lock.Reset();

                parentPackage->MarkDirty();
            }
        }
        
        lock.Reset();

        if (!parentPackage)
        {
            lock.Reset(m_mutex);

            auto it = m_packages.Find(package->GetName());
            Assert(it != m_packages.End());

            m_packages.Erase(it);

            removed = true;
        }
    }

    if (removed)
    {
        OnPackageRemoved(strongPackage);

        return;
    }
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist)
{
    HYP_SCOPE;

    String assetName;

    return GetPackageFromPath_Internal(path, AssetRegistryPathType::PACKAGE, createIfNotExist, assetName);
}

Handle<AssetPackage> AssetRegistry::GetSubpackage(
    const Handle<AssetPackage>& parentPackage,
    Name subpackageName,
    bool createIfNotExist)
{
    HYP_SCOPE;

    Handle<AssetPackage> pkg;

    bool isSubpackageSaved = false;

    if (!parentPackage)
    {
        {
            TUniqueLock guard(m_mutex);

            auto packageIt = m_packages.Find(subpackageName);

            if (packageIt != m_packages.End())
            {
                pkg = *packageIt;
            }
            else
            {
                // Try loading from manifest path, if it exists.
                FilePath subpackageDir = g_assetManager->GetBasePath() / *subpackageName;
                FilePath manifestPath = subpackageDir / "PackageManifest.json";

                if (manifestPath.Exists() && !manifestPath.IsDirectory())
                {
                    // same here, need to break out of mutex
                    guard.Reset();

                    // note forceLoad is true here
                    TResult<Handle<AssetPackage>> loadResult = LoadPackageFromManifest(manifestPath, /* loadSubpackages */ false, /* forceLoad */ true);

                    guard.Reset(m_mutex);

                    if (loadResult.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load package '{}' from manifest '{}': {}",
                            subpackageName, manifestPath, loadResult.GetError().GetMessage());
                    }
                    else
                    {
                        pkg = std::move(*loadResult);

                        if (pkg)
                        {
                            pkg->m_registry = WeakHandleFromThis();

                            InitObject(pkg);

                            m_packages.Insert(pkg);
                        }
                    }
                }
                else if (createIfNotExist)
                {
                    pkg = MakeHandle<AssetPackage>(subpackageName);
                    pkg->m_registry = WeakHandleFromThis();

                    InitObject(pkg);

                    pkg->MarkDirty();

                    m_packages.Insert(pkg);
                }

                OnPackageAdded(pkg);
            }
        }

        return pkg;
    }

    {
        TUniqueLock guard(parentPackage->m_mutex);

        auto packageIt = parentPackage->m_subpackages.Find(subpackageName);

        if (packageIt != parentPackage->m_subpackages.End())
        {
            pkg = *packageIt;
        }
        else
        {
            // Try load from manifest path
            FilePath subpackageDir = parentPackage->m_packageDir / *subpackageName;
            FilePath manifestPath = subpackageDir / "PackageManifest.json";

            if (manifestPath.Exists() && !manifestPath.IsDirectory())
            {
                // need to break out of the mutex for LoadPackageFromManifest()
                guard.Reset();

                TResult<Handle<AssetPackage>> loadResult = LoadPackageFromManifest(manifestPath, /* loadSubpackages */ false, /* forceLoad */ true);
                
                guard.Reset(parentPackage->m_mutex);

                if (loadResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to load subpackage '{}' from manifest '{}': {}",
                        subpackageName, manifestPath, loadResult.GetError().GetMessage());
                }
                else
                {
                    pkg = std::move(*loadResult);

                    if (pkg)
                    {
                        pkg->m_parentPackage = parentPackage;
                        pkg->m_flags |= parentPackage->m_flags;

                        InitObject(pkg);

                        parentPackage->m_subpackages.Insert(pkg);
                        parentPackage->OnSubpackageAdded(pkg);
                    }
                }
            }
            else if (createIfNotExist)
            {
                pkg = MakeHandle<AssetPackage>(subpackageName);
                pkg->m_registry = WeakHandleFromThis();
                pkg->m_parentPackage = parentPackage;
                pkg->m_flags |= parentPackage->m_flags;

                // If parent package exists on disk, save this package:
                if (!parentPackage->IsTransient() && parentPackage->IsSaved_Internal())
                {
                    FilePath subpackageDir = parentPackage->m_packageDir / *subpackageName;

                    if (ShouldSavePackageOnChanged(*parentPackage))
                    {
                        Result savePackageResult = pkg->Save(subpackageDir, /* saveEvenIfNotDirty*/ true);
                        if (!savePackageResult.HasError())
                        {
                            isSubpackageSaved = true;
                        }
                        else
                        {
                            HYP_LOG(Assets, Error, "Failed to save subpackage {} of {}: {}",
                                pkg->GetName(), parentPackage->BuildPackagePath(), savePackageResult.GetError().GetMessage());
                        }
                    }
                }

                InitObject(pkg);

                parentPackage->m_subpackages.Insert(pkg);
                parentPackage->OnSubpackageAdded(pkg);
                
                // exit mutex to mark dirty
                guard.Reset();

                if (!isSubpackageSaved)
                {
                    pkg->MarkDirty();
                }
            }
        }
    }

    return pkg;
}

void AssetRegistry::LoadSubpackages(const Handle<AssetPackage>& package, bool recursive)
{
    HYP_SCOPE;

    if (!package)
    {
        HYP_LOG(Assets, Warning, "Cannot load subpackages for null package");
        return;
    }

    if (package->m_registry.GetUnsafe() != this)
    {
        HYP_LOG(Assets, Warning, "Cannot load subpackages for package '{}' that does not belong to this AssetRegistry", package->GetName());
        return;
    }

    if (package->m_packageDir.Length() == 0 || !package->m_packageDir.Exists() || !package->m_packageDir.IsDirectory())
    {
        return;
    }

    for (const FilePath& subdirectory : package->m_packageDir.GetSubdirectories())
    {
        const FilePath manifestPath = subdirectory / "PackageManifest.json";

        if (!manifestPath.Exists() || manifestPath.IsDirectory())
        {
            continue;
        }

        TResult<Handle<AssetPackage>> subpackageResult = LoadPackageFromManifest(manifestPath, /* loadSubpackages */ false, /* forceLoad */ false);

        if (subpackageResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", manifestPath, subpackageResult.GetError().GetMessage());

            continue;
        }

        Handle<AssetPackage> subpackage = std::move(*subpackageResult);

        if (!subpackage.IsValid())
        {
            HYP_LOG(Assets, Error, "Subpackage at path '{}' is invalid!", subdirectory);

            continue;
        }

        if (!subpackage->GetName().IsValid())
        {
            HYP_LOG(Assets, Error, "Subpackage at path '{}' has an invalid name!", subdirectory);

            continue;
        }

        subpackage->m_parentPackage = package;
        subpackage->m_flags |= package->m_flags;

        // Add to our package
        Handle<AssetPackage> existingSubpackage = GetSubpackage(package, subpackage->GetName(), /* createIfNotExist */ true);
        Assert(existingSubpackage != nullptr);

        if (existingSubpackage != subpackage)
        {
            HYP_LOG(Assets, Warning, "Subpackage with name '{}' already exists in package '{}', skipping loaded subpackage from '{}'",
                subpackage->GetName(), package->BuildPackagePath(), manifestPath);
        }
    }
}

TResult<Handle<AssetPackage>> AssetRegistry::LoadPackageFromManifest(
    const FilePath& manifestPath,
    bool loadSubpackages,
    bool forceLoad)
{
    HYP_SCOPE;

    HYP_LOG(Assets, Debug, "Loading package from manifest path: {}", manifestPath);

    Handle<AssetPackage> outPackage;

    if (!manifestPath.Exists() || manifestPath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Manifest file '{}' does not exist or is not a file", manifestPath);
    }

    const FilePath dir = manifestPath.BasePath();

    FileBufferedReaderSource manifestSource { manifestPath };
    BufferedReader manifestStream { &manifestSource };

    if (!manifestStream.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file '{}'", manifestPath);
    }

    JSON::ParseResult parseResult = JSON::Parse(manifestStream);

    manifestStream.Close();

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    if (!parseResult.value.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Package manifest JSON must be an object, but got value: {}", parseResult.value.ToString());
    }

    const String packagePath = parseResult.value.Get("Path").ToString().ToUtf8();
    const String packageName = parseResult.value.Get("Name").ToString().ToUtf8();

    if (packagePath.Empty() || packageName.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Package manifest JSON does not contain a valid 'Path' or 'Name' field");
    }

    if (!forceLoad)
    {
        // try to get existing package if not forcing load
        if (Handle<AssetPackage> existingPackage = GetPackageFromPath(packagePath, /* createIfNotExist */ false); existingPackage.IsValid())
        {
            return existingPackage;
        }
    }

    Handle<AssetPackage> parentPackage;

    {
        // Have to create package first so we can deserialize into it
        // we also need to load all parent packages until we reach a package that already exists.

        // build parent packages first
        Array<String> parentPackageParts = packagePath.Split('/');
        parentPackageParts.PopBack(); // remove last part (our own package name)

        // get parent package path
        if (parentPackageParts.Any())
        {
            const String parentPackagePathString = String::Join(parentPackageParts, '/');

            // check if parent package already exists first
            parentPackage = GetPackageFromPath(parentPackagePathString, /* createIfNotExist */ false);

            if (!parentPackage)
            {
                const String relativePath = AssetPath::MakeRelativePath(AssetPath(packagePath), AssetPath(parentPackagePathString));

                // make filepath for parent package manifest
                const FilePath parentManifestPath = dir / relativePath / "PackageManifest.json";

                HYP_LOG(Assets, Debug, "Loading parent package '{}' for package at '{}' from manifest '{}'", parentPackagePathString, packagePath, parentManifestPath);

                // attempt to load parent package from manifest
                if (TResult<Handle<AssetPackage>> parentPackageResult = LoadPackageFromManifest(parentManifestPath, /* loadSubpackages */ false, /* forceLoad */ true); parentPackageResult.HasError())
                {
                    return HYP_MAKE_ERROR(Error, "Failed to load parent package '{}' for package at '{}' from manifest '{}': {}", parentPackagePathString, packagePath, parentManifestPath, parentPackageResult.GetError().GetMessage());
                }
                else
                {
                    parentPackage = *parentPackageResult;
                }
            }
        }
    }

    outPackage = MakeHandle<AssetPackage>(CreateNameFromDynamicString(packageName));
    outPackage->m_registry = WeakHandleFromThis();
    outPackage->m_packageDir = dir;
    outPackage->m_parentPackage = parentPackage;
    outPackage->m_isLoading = true;

    if (parentPackage)
    {
        outPackage->m_flags |= parentPackage->m_flags;
    }

    HYP_DEFER({
        if (outPackage)
        {
            outPackage->m_isLoading = false;
        }
    });

    {
        BoxedValue packageData = BoxedValue(outPackage);

        if (!ObjectFromJSON(parseResult.value.AsObject(), outPackage->InstanceClass(), packageData))
        {
            HYP_LOG(Assets, Error, "Failed to deserialize package manifest JSON for package at '{}'", manifestPath);

            return HYP_MAKE_ERROR(Error, "Failed to load package data from manifest");
        }
    }

    // Load dependency packages first (always, regardless of loadSubpackages flag)
    // Dependencies must be loaded before assets to ensure all referenced assets via AssetReferences exist in the registry
    for (const AssetPath& dependencyPath : outPackage->GetDependencies())
    {
        if (!dependencyPath.IsValid())
        {
            HYP_LOG(Assets, Warning, "Invalid dependency path in package '{}'", outPackage->GetName());
            continue;
        }

        HYP_LOG(Assets, Debug, "Loading dependency package '{}' for package '{}'", dependencyPath, outPackage->GetName());

        Handle<AssetPackage> dependencyPackage = GetPackageFromPath(dependencyPath.ToString(), /* createIfNotExist */ false);

        if (dependencyPackage != nullptr)
        {
            HYP_LOG(Assets, Debug, "Dependency package '{}' already loaded!", dependencyPath);
            continue;
        }

        // Dependency package doesn't exist yet, try to load it from filesystem
        const String relativePath = AssetPath::MakeRelativePath(AssetPath(outPackage->BuildPackagePath()), dependencyPath);
        const FilePath dependencyManifestPath = GetResourceDirectory() / FilePath::Relative(dir / relativePath / "PackageManifest.json", GetResourceDirectory());

        if (dependencyManifestPath.Exists() && !dependencyManifestPath.IsDirectory())
        {
            HYP_LOG(Assets, Debug, "Loading dependency package '{}' from manifest '{}'", dependencyPath, dependencyManifestPath);

            TResult<Handle<AssetPackage>> dependencyPackageResult = LoadPackageFromManifest(dependencyManifestPath, /* loadSubpackages */ false, /* forceLoad */ true);

            if (dependencyPackageResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to load dependency package '{}' from manifest '{}': {}", dependencyPath, dependencyManifestPath, dependencyPackageResult.GetError().GetMessage());
                continue;
            }

            dependencyPackage = std::move(*dependencyPackageResult);
        }
        else
        {
            HYP_LOG(Assets, Error, "Dependency package '{}' for package '{}' not found at '{}'", dependencyPath, outPackage->GetName(), dependencyManifestPath);
            continue;
        }
    }

    // get asset manifest ifles
    Array<FilePath> assetFiles;

    for (const FilePath& path : dir.GetAllFilesInDirectory())
    {
        if (path.GetExtension() != "json")
        {
            continue;
        }

        if (path.Basename() == "PackageManifest.json")
        {
            // Skip the package manifest itself
            continue;
        }

        assetFiles.PushBack(path);
    }

    // load AssetObjects from manifest files in this package
    if (assetFiles.Any())
    {
        Array<Handle<AssetObject>> assetObjects;
        assetObjects.Resize(assetFiles.Size());

        //// load assets in parallel
        //TaskSystem::GetInstance().ParallelForEach(assetFiles, [&assetObjects](const FilePath& entry, uint32 index, uint32) -> void
        //    {
        for (uint32 index = 0; index < uint32(assetFiles.Size()); index++)
        {
            const FilePath& entry = assetFiles[index];

            FileBufferedReaderSource manifestSource { entry };
            BufferedReader manifestStream { &manifestSource };

            const FilePath binPath = entry.StripExtension();

            BufferedReader* dataStream = nullptr;
            FileBufferedReaderSource* dataSource = nullptr;

            if (binPath.Exists() && !binPath.IsDirectory())
            {
                dataSource = new FileBufferedReaderSource { binPath };
                dataStream = new BufferedReader { dataSource };
            }

            HYP_DEFER({
                if (dataStream)
                {
                    dataStream->Close();
                    delete dataStream;
                }

                if (dataSource)
                {
                    delete dataSource;
                }
            });

            Handle<AssetObject> assetObject;

            if (Result loadAssetResult = AssetObject::Load(manifestStream, dataStream, assetObject); loadAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to load asset from manifest '{}': {}", entry, loadAssetResult.GetError().GetMessage());

                continue;
            }

            assetObject->m_manifestPath = entry;

            assetObjects[index] = std::move(assetObject);
        }

        for (const Handle<AssetObject>& assetObject : assetObjects)
        {
            if (!assetObject)
            {
                continue;
            }

            // ensure we call PostLoad on the original thread we called this from (or the sim thread if UseSingleThread is true)
            assetObject->InstanceClass()->PostLoad(assetObject.Get());

            if (Result addAssetResult = outPackage->AddAssetObject(assetObject); addAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to add asset to package '{}': {}", outPackage->GetName(), addAssetResult.GetError().GetMessage());

                continue;
            }
        }
    }

    // Load subpackages after assets (if requested)
    // Each subpackage will be loaded with loadSubpackages=true to recursively load their children
    if (loadSubpackages)
    {
        for (const FilePath& subdirectory : dir.GetSubdirectories())
        {
            for (const FilePath& entry : subdirectory.GetAllFilesInDirectory())
            {
                if (entry.Basename() == "PackageManifest.json")
                {
                    // Load WITH sub-subpackages recursively
                    TResult<Handle<AssetPackage>> subpackageResult = LoadPackageFromManifest(entry, /* loadSubpackages */ true, /* forceLoad */ false);

                    if (subpackageResult.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", entry, subpackageResult.GetError().GetMessage());
                        break;
                    }

                    Handle<AssetPackage> subpackage = std::move(*subpackageResult);

                    if (subpackage.IsValid())
                    {
                        subpackage->m_parentPackage = outPackage;
                        subpackage->m_flags |= outPackage->m_flags;

                        outPackage->m_subpackages.Insert(subpackage);
                        outPackage->OnSubpackageAdded(subpackage);
                    }

                    break;
                }
            }
        }
    }

    outPackage->m_isLoading = false;

    InitObject(outPackage);

    if (parentPackage != nullptr)
    {
        TUniqueLock guard(parentPackage->m_mutex);
        parentPackage->m_subpackages.Insert(outPackage);
        parentPackage->OnSubpackageAdded(outPackage);
    }
    else // top-level package
    {
        TUniqueLock guard(m_mutex);

        m_packages.Insert(outPackage);
        OnPackageAdded(outPackage);
    }

    return outPackage;
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::Char32('/') || *it == utf::Char32('\\'))
        {
            currentPackage = GetSubpackage(currentPackage, CreateNameFromDynamicString(currentString), createIfNotExist);

            currentString.Clear();

            if (!currentPackage)
            {
                return Handle<AssetPackage>::empty;
            }

            continue;
        }

        currentString.Append(*it);
    }

    switch (pathType)
    {
    case AssetRegistryPathType::PACKAGE:
        outAssetName.Clear();

        // If it is a PACKAGE path, if there is any remaining string, get / create the subpackage
        if (!currentPackage.IsValid() || currentString.Any())
        {
            currentPackage = GetSubpackage(currentPackage, CreateNameFromDynamicString(currentString), createIfNotExist);
        }

        break;
    case AssetRegistryPathType::ASSET:
        outAssetName = std::move(currentString);

        break;
    default:
        HYP_UNREACHABLE();
    }

    return currentPackage;
}

Name AssetRegistry::GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const
{
    HYP_SCOPE;

    Handle<AssetPackage> package = const_cast<AssetRegistry*>(this)->GetPackageFromPath(packagePath, /* createIfNotExist */ false);

    if (!package.IsValid())
    {
        return baseName;
    }

    return package->GetUniqueAssetName(baseName);
}

Result AssetRegistry::RegisterAsset(const UTF8StringView& path, const Handle<AssetObject>& assetObject)
{
    HYP_SCOPE;

    if (!assetObject.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    Array<String> pathStringSplit = String(path).Split('/', '\\');

    String pathString = String::Join(pathStringSplit, '/');

    AssetRegistryPathType pathType = AssetRegistryPathType::PACKAGE;

    Handle<AssetPackage> assetPackage;

    {
        String assetName;
        assetPackage = GetPackageFromPath_Internal(pathString, pathType, /* createIfNotExist */ true, assetName);

        if (pathType == AssetRegistryPathType::ASSET)
        {
            const Name baseName = assetName.Any() ? CreateNameFromDynamicString(assetName) : NAME("Unnamed");

            assetObject->m_name = baseName;
        }
    }

    const Name desiredName = assetObject->GetName();
    const bool renameOnNameClash = ShouldUniquifyAssetNames(*assetPackage);

    if (assetPackage->HasAssetWithName(desiredName))
    {
        if (renameOnNameClash)
        {
            const Name uniqueName = assetPackage->GetUniqueAssetName(desiredName);

            if (Result renameResult = assetObject->Rename(uniqueName); renameResult.HasError())
            {
                return renameResult;
            }
        }
        else
        {
            if (Handle<AssetObject> existingAssetObject = assetPackage->GetAssetObject(desiredName); existingAssetObject.IsValid())
            {
                // remove old asset and overwrite it
                if (Result removeResult = assetPackage->RemoveAssetObject(existingAssetObject); removeResult.HasError())
                {
                    return removeResult;
                }
            }
        }
    }

    return assetPackage->AddAssetObject(assetObject);
}

void AssetRegistry::RegisterAssetsRecursively(
    const UTF8StringView& packagePath,
    const BoxedValue& target,
    bool forceRelocation,
    ProcRef<String(const AssetObject&)> getObjectSubpath)
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

    if (!target.IsValid() || target.IsNull())
    {
        return;
    }

    //// \todo : Change to a Stack, recursion could get impressively deep.

    HashSet<const ObjectBase*> visited; // to avoid infinite recursion

    bool shouldFollowAssetPaths = false;

    auto HandleAssetReference = [this](const AssetReference& assetReference, AssetPackage& package)
    {
        Array<Name> chain = assetReference.GetAssetPath().GetChain();

        if (chain.Size() > 1) // has at least one package in chain
        {
            chain.PopBack(); // remove asset name

            const String packagePath = String::Join(chain, '/', &Name::ToString);
            const Handle<AssetPackage> referencedPackage = GetPackageFromPath(packagePath, /* createIfNotExist */ false);

            if (referencedPackage.IsValid() && !referencedPackage->IsSubpackageOf(package))
            {
                package.AddDependency(AssetPath(packagePath));
            }
        }
    };

    Proc<void(const Handle<AssetPackage>&, const BoxedValue&)> iterate;
    iterate = [&](const Handle<AssetPackage>& inPackage, const BoxedValue& current) -> void
    {
        Assert(inPackage != nullptr);

        if (!current.IsValid() || current.IsNull())
        {
            return;
        }

        {
            ObjectBase* object = current.TryGet<ObjectBase*>().GetOr(nullptr);
            if (object && !visited.Insert(object).second)
            {
                HYP_LOG(Assets, Info, "Already visited {} with ID {}, skipping to avoid infinite recursion",
                    object->InstanceClass() ? *object->InstanceClass()->GetName() : "<no class>", object->Id());

                return;
            }
        }

        Handle<AssetPackage> parentPackage = inPackage;
        Handle<AssetObject> assetObject;

        Optional<AssetReference> tmpAssetReference;
        const AssetReference* assetReference = nullptr;

        if (current.Is<AssetObject>())
        {
            assetObject = MakeStrongRef(&current.Get<AssetObject>());
            Assert(assetObject != nullptr);

            assetReference = &tmpAssetReference.Emplace(assetObject);
        }
        else if (current.Is<AssetPath>() && shouldFollowAssetPaths)
        {
            assetReference = &tmpAssetReference.Emplace(current.Get<AssetPath>());
        }
        else if (current.Is<AssetReference>())
        {
            assetReference = &current.Get<AssetReference>();
        }

        if (assetReference && !assetReference->IsValid())
        {
            assetReference = nullptr;
        }

        if (assetReference && !assetObject)
        {
            assetObject = assetReference->Resolve();

            if (!assetObject)
            {
                HYP_LOG(Assets, Warning, "AssetReference {} failed to resolve!", assetReference->GetAssetPath().ToString());
            }
        }

        if (assetObject && ShouldRelocateAssetBeforeSave(*assetObject))
        {
            TResult<Handle<AssetPackage>> relocateResult = HYP_MAKE_ERROR(Error, "Asset relocated failed unexpectedly");

            if (assetObject->IsRegistered()) // already has a path but is transient e.g $Memory/Media/Meshes/Foo; needs to be moved to NewPackage/Media/Meshes/Foo
            {
                relocateResult = RelocateAsset(*this, assetObject, packagePath, /* preserveStructure */ true);
            }
            else // Doesn't have a path; register instance with package using the passed in function to decide where to relocate it to.
            {
                const String packagePathWithSubpath = getObjectSubpath
                    ? packagePath + "/" + getObjectSubpath(*assetObject)
                    : String(packagePath);

                relocateResult = RelocateAsset(*this, assetObject, packagePathWithSubpath, /* preserveStructure */ false);

#if 0
                Handle<AssetPackage> newPackage = GetPackageFromPath(packagePathWithSubpath, /* createIfNotExist */ true);
                Assert(newPackage != nullptr);

                Handle<AssetPackage> prevPackage = assetObject->GetPackage();

                HYP_LOG(Assets, Debug, "Relocating asset {} to {}",
                    prevPackage ? prevPackage->BuildAssetPath(assetObject->GetName()).ToString() : String(),
                    newPackage->BuildPackagePath());

                // try to move it
                if (Result result = newPackage->AddAssetObject(assetObject); result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to relocate {} {} from temp/transient package '{}' to '{}': {}",
                        assetObject->InstanceClass()->GetName(),
                        assetObject->GetName(),
                        prevPackage ? prevPackage->BuildPackagePath() : "<no package>",
                        newPackage->BuildPackagePath(),
                        result.GetError().GetMessage());

                    return;
                }

                HYP_LOG(Assets, Debug, "Moved {} {} from temp/transient package {} to {}",
                    assetObject->InstanceClass()->GetName(),
                    assetObject->GetName(),
                    prevPackage ? prevPackage->BuildPackagePath() : "<no package>",
                    newPackage->BuildPackagePath());

                if (!newPackage->IsSubpackageOf(*inPackage))
                {
                    inPackage->AddDependency(AssetPath(packagePathWithSubpath));
                }

                parentPackage = std::move(newPackage);
            }
            
            if (relocateResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to asset: {}", relocateResult.GetError().GetMessage());
                return;
            }

            Handle<AssetPackage> newPackage = std::move(*relocateResult);

            if (!newPackage->IsSubpackageOf(*inPackage))
            {
                inPackage->AddDependency(AssetPath(newPackage->BuildPackagePath()));
            }

            parentPackage = std::move(newPackage);
#endif
            }
        }
        else if (assetReference)
        {
            HandleAssetReference(*assetReference, *inPackage);
        }

        shouldFollowAssetPaths = false;

        // @TODO Move array, entity, streaming cell special handlings out of this function:
        // like some kind of handler defined per class

        if (current.IsArray()) // array needs special handling: iterate over elements (if possible)
        {
            GenericArrayWrapper& array = current.Get<GenericArrayWrapper>();

            if (!array.CanGetElementByIndex())
            {
                HYP_LOG(Assets, Error, "Cannot iterate over {}: not indexable", LookupTypeName(current.GetTypeId()));
                return;
            }

            SizeType size = array.Size();

            for (SizeType i = 0; i < size; ++i)
            {
                BoxedValue boxed;
                if (!array.GetElementAt(i, boxed))
                {
                    HYP_LOG(Assets, Warning, "Failed to get element at index {} of array of type {}", i, LookupTypeName(current.GetTypeId()));
                    continue;
                }

                iterate(parentPackage, boxed);
            }

            return;
        }

        // special handling for Entity: needs to collect from components
        /// \todo : Move to a method that can be overridden for custom handling?
        if (current.Is<Entity>())
        {
            const Entity& entity = current.Get<Entity>();

            EntityManager* entityManager = entity.GetEntityManager();
            if (entityManager != nullptr)
            {
                const auto componentIds = entityManager->GetAllComponents(&entity);
                if (componentIds.HasValue())
                {
                    for (const auto& [typeId, componentId] : *componentIds)
                    {
                        const AnyRef componentRef = entityManager->TryGetComponent(typeId, &entity);
                        if (!componentRef.HasValue())
                        {
                            continue;
                        }

                        iterate(parentPackage, BoxedValue(componentRef));
                    }
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Entity {} has no valid EntityManager, cannot iterate components", entity.Id());
            }
        }

        if (current.Is<StreamingCell>())
        {
            const StreamingCell& streamingCell = current.Get<StreamingCell>();

            for (const AssetReference& assetReference : streamingCell.GetAssetReferences())
            {
                if (IsTransientAssetPath(assetReference.GetAssetPath()))
                {
                    HYP_LOG(Assets, Error, "StreamingCell contains a reference to the asset: {}, which is in an in-memory package or the $Temp package on the filesystem.\n"
                        "This may result in issues with loading the asset later down the line.",
                        assetReference.GetAssetPath().ToString());
                }
            }
        }
        
        const Class* cls = GetClass(current.GetTypeId());

        const BoxedValue* boxed = &current;
        BoxedValue tmpBoxed;

        if (assetObject != nullptr)
        {
            tmpBoxed = BoxedValue(assetObject);
            boxed = &tmpBoxed;

            cls = assetObject->InstanceClass();
        }

        if (!cls) // no Class; not an object we can iterate over.
        {
            return;
        }

        for (const IMember& member : cls->GetMembers(MemberType::Property | MemberType::Field, /* deep */ true))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                // skip non-property members if they have the 'property' attribute (synthetic property)
                continue;
            }

            if (member.GetAttribute(Attributes::g_attrTransient).GetBool()
                || !member.GetAttribute(Attributes::g_attrSerialize).GetBool(true))
            {
                continue;
            }

            BoxedValue memberData;
            switch (member.GetMemberType())
            {
            case MemberType::Property:
            {
                const Property* property = static_cast<const Property*>(&member);
                memberData = property->Get(*boxed);
                break;
            }
            case MemberType::Field:
            {
                const Field* field = static_cast<const Field*>(&member);
                memberData = field->Get(*boxed);
                break;
            }
            default:
                HYP_UNREACHABLE();
                break;
            }

            if (!memberData.IsValid() || memberData.IsNull())
            {
                continue;
            }

            const Class* memberClass = memberData.GetTypeInfo()->GetClass();
            if (memberClass != nullptr && !memberClass->GetAttribute(Attributes::g_attrSerialize).GetBool(true))
            {
                // skip members with Serialize=false on class
                continue;
            }

            shouldFollowAssetPaths = member.GetAttribute(Attributes::g_attrFollowAssetPath).GetBool();

            iterate(parentPackage, memberData);
        }

        if (assetObject != nullptr)
        {
            if (forceRelocation || !assetObject->IsRegistered())
            {
                if (Result result = parentPackage->AddAssetObject(assetObject); result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to register asset '{}': {}", assetObject->GetName(), result.GetError().GetMessage());
                }
            }
        }
    };

    Handle<AssetPackage> rootPackage = GetPackageFromPath(packagePath, /* createIfNotExist */ true);
    Assert(rootPackage.IsValid());

    iterate(rootPackage, target);
}

Handle<AssetObject> AssetRegistry::GetAssetFromPath(const UTF8StringView& path) const
{
    HYP_SCOPE;

    String assetName;

    Handle<AssetPackage> package = const_cast<AssetRegistry*>(this)->GetPackageFromPath_Internal(path, AssetRegistryPathType::ASSET, /* createIfNotExist */ false, assetName);

    if (!package.IsValid() || !assetName.Any())
    {
        return Handle<AssetObject>::empty;
    }

    return package->GetAssetObject(assetName);
}

#pragma endregion AssetRegistry

} // namespace Hyperion
