/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetReference.hpp>
#include <asset/Assets.hpp>
#include <asset/BlobStorage.hpp>
#include <asset/BlobStorageViews.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/GlobalContext.hpp>
#include <Core/reflection/TypeInfo.hpp>

#include <Core/threading/Scheduler.hpp>

#include <Core/serialization/SerializationUtils.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/Property.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/json/JSON.hpp>

#include <Core/config/Config.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>

#include <streaming/StreamingCell.hpp>

#include <engine/EngineDriver.hpp>

#include <AssetRegistry.generated.inl>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
extern HYP_NODISCARD FilePath CreateTempDirectory();
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

HYP_API extern const FilePath& GetCacheDirectory();

static const ThreadId& s_assetRegistryThread = g_simThread;

static constexpr const char* BlobStorageName = "Storage";

// If true, all mutation operations will be forced to run on the sim thread,
// otherwise a mutex will be used to allow multi-threaded access.
static constexpr bool UseSingleThread = false;

static constexpr const StringHash PredefinedPackages[] = {
    "Engine"_sh
};

static constexpr const StringHash PredefinedTransientPackages[] = {
    "$Memory"_sh,
    "$Temp"_sh,
    "$Import"_sh
};

static constexpr const StringHash RelocatablePackages[] = {
    "$Memory"_sh,
    "$Temp"_sh,
    "$Import"_sh
};

HYP_API extern const FilePath& GetLibraryDirectory();

#if HYP_EDITOR
HYP_API extern const FilePath& GetProjectsDirectory();
#endif

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

static bool IsRelocatable(const AssetPath& assetPath)
{
    if (!assetPath)
    {
        return false;
    }

    Name rootPackageName = assetPath.chain[0];

    return std::find(
        std::begin(RelocatablePackages),
        std::end(RelocatablePackages),
        StringHash(rootPackageName)) != std::end(RelocatablePackages);
}

/*! \brief Is the AssetObject located in a package that allows us to move it elsewhere?
 *  Only Engine-defined internal packages (e.g $Memory, $Import, $Temp) enable this behaviour. */
static bool ShouldRelocateAssetBeforeSave(const AssetObject& assetObject)
{
    if (assetObject.GetAssetFlags() & AssetObjectFlags::Transient)
    {
        return false; // explicitly marked transient; don't move
    }

    return !assetObject.IsRegistered() || IsRelocatable(assetObject.GetPath());
}

template <size_t Size>
static bool IsPackageInList(const AssetPackage& package, const StringHash(&elems)[Size], bool matchSubpackages = true)
{
    StringHash substrHash = StringHash(package.GetName());

    if (matchSubpackages)
    {
        const String packagePath = package.BuildPackagePath();
        const UTF8StringView substr = packagePath.Substr(0, packagePath.FindFirstIndex('/'));
        substrHash = StringHash(substr);
    }

    for (size_t i = 0; i < Size; i++)
    {
        StringHash packageName = elems[i];

        if (substrHash == packageName)
        {
            return true;
        }
    }

    return false;
}


template <size_t Size>
static bool IsPackageInList(const UTF8StringView& packagePath, const StringHash(&elems)[Size])
{
    const UTF8StringView substr = packagePath.Substr(0, packagePath.FindFirstIndex('/'));
    
    const StringHash substrHash = StringHash(substr);

    for (size_t i = 0; i < Size; i++)
    {
        StringHash packageName = elems[i];

        if (substrHash == packageName)
        {
            return true;
        }
    }

    return false;
}

template <size_t Size>
static bool IsPackageInList(Name name, const StringHash(&elems)[Size])
{
    for (size_t i = 0; i < Size; i++)
    {
        StringHash packageName = elems[i];

        if (name == packageName)
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

    return IsPackageInList(package, PredefinedPackages)
        || IsPackageInList(package, PredefinedTransientPackages);
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
        AssetPackage* currentPackage = previousPackage;

        while (currentPackage != nullptr && !IsPackageInList(*currentPackage, RelocatablePackages, /* matchSubpackages */ false))
        {
            subpackageNames.PushBack(currentPackage->GetName());
            currentPackage = currentPackage->GetParentPackage();
        }

        subpackageNames.Reverse();

        newPath = String(newPackageBasePath) + '/' + String::Join(subpackageNames, '/', &Name::LookupString);
    }
    else
    {
        newPath = newPackageBasePath;
    }

    HYP_LOG(Assets, Verbose, "Relocating asset '{}' to: '{}'", assetObject->GetName(), newPath);

    if (Result registerAssetResult = registry.RegisterAsset(newPath, assetObject, AddAssetConflictMode::FailOnConflict); registerAssetResult.HasError())
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

static Result ReadManifest(ByteReader& stream, const FilePath& manifestPath, JSON::Object& outManifestData)
{
    String str = String(stream.Read().ToByteView());

    JSON::ParseResult parseResult = JSON::Parse(str);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    JSON::Value manifestJson = std::move(parseResult.value);

    if (!manifestJson.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON at path {} is not a valid JSON object:\n{}",
            manifestPath, manifestJson.ToString(true));
    }

    outManifestData = std::move(manifestJson.AsObject());

    return {}; // ok
}

#pragma region AssetPackage

AssetPackage::AssetPackage()
    : AssetPackage(Name::Invalid())
{
}

AssetPackage::AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags)
    : m_parentPackage(nullptr),
      m_flags(flags),
      m_stateFlags(0),
      m_lastSavedTimestamp(Time(0))
{
    if (name.IsValid())
    {
        if (IsPackageInList(name, PredefinedTransientPackages))
        {
            m_flags |= AssetPackageFlags::Hidden | AssetPackageFlags::Transient;
        }
        else
        {
            const char* str = name.LookupString();

            if (str[0] == '$')
            {
                m_flags |= AssetPackageFlags::Hidden;
            }
        }

        m_name = SanitizeName(name);
        m_friendlyName = CreateFriendlyName(name);
    }
}

AssetPackage::~AssetPackage()
{
    TUniqueLock lock(m_mutex);
    for (const Handle<AssetObject>& assetObject : m_assetObjects)
    {
        if (!assetObject.IsValid())
            continue;

        assetObject->OnUnloaded();
    }
}

void AssetPackage::Init()
{
    HYP_SCOPE;

    Handle<AssetRegistry> registry = m_registry.Lock();
    Assert(registry.IsValid());

    Array<Handle<AssetObject>> assetObjects;
    Array<Handle<AssetPackage>> subpackages;

    HashSet<AssetObject*> assetObjectsToSave;

    bool isLoading = false;
    bool isPackageSavedInFilesystem = false;
    bool shouldSaveAssets = false;

    FilePath packageDir;

    { // lock scope (shared)
        TSharedLock guard(m_mutex);

        packageDir = m_packageDir;

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
            
            assetObject->m_package = WeakHandleFromThis();
            assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

            InitObject(assetObject);

            assetObjects.PushBack(assetObject);
        }

        for (const Handle<AssetPackage>& subpackage : m_subpackages)
        {
            InitObject(subpackage);

            if (isPackageSavedInFilesystem && shouldSaveAssets)
            {
                FilePath subpackageDir = packageDir / *subpackage->GetName();

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

        if (!shouldSaveAssets && !isLoading && assetObjects.Any())
        {
            MarkDirty(); // if not saving assets right now, need to mark it to be saved later
        }
    }

    for (const Handle<AssetObject>& assetObject : assetObjects)
    {
        if (shouldSaveAssets && assetObjectsToSave.Contains(assetObject.Get()))
        {
            AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
                
            const FilePath newManifestFilepath = packageDir / *assetObject->GetName() + ".json";

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

        AssetPackage* parentPackage = m_parentPackage;

        while (parentPackage != nullptr)
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage();
        }
    }

    SetReady(true);
}

bool AssetPackage::IsSubpackageOf(const AssetPackage& other) const
{
    HYP_SCOPE;

    AssetPackage* parentPackage = m_parentPackage;

    while (parentPackage != nullptr)
    {
        if (parentPackage == &other)
        {
            return true;
        }

        parentPackage = parentPackage->GetParentPackage();
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
        OnAssetObjectRemoved(assetObject, true);

        AssetPackage* parentPackage = m_parentPackage;

        while (parentPackage != nullptr)
        {
            parentPackage->OnAssetObjectRemoved(assetObject, false);
            parentPackage = parentPackage->GetParentPackage();
        }
        
        assetObject->m_package.Reset();
    }

    previousAssetObjects.Clear();

    Array<Handle<AssetObject>> newAssetObjects;
    HashSet<AssetObject*> assetObjectsToSave;
    
    bool isLoading = false;
    bool shouldSaveAssets = false;
    bool isPackageSavedInFilesystem = false;

    FilePath packageDir;

    { // lock scope (unique since we assign asset objects)
        TUniqueLock guard(m_mutex);
        
        packageDir = m_packageDir;
        
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

        if (!isLoading && !shouldSaveAssets)
        {
            MarkDirty();
        }
    } // end lock scope

    for (const Handle<AssetObject>& assetObject : newAssetObjects)
    {
        if (isLoading)
        {
            assetObject->SetIsTransientByProxy(false);
        }
        else if (shouldSaveAssets && assetObjectsToSave.Contains(assetObject.Get()))
        {
            AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
                
            const FilePath newManifestFilepath = packageDir / *assetObject->GetName() + ".json";

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

        AssetPackage* parentPackage = m_parentPackage;

        while (parentPackage != nullptr)
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage();
        }
    }
}

Result AssetPackage::AddAssetObject(const Handle<AssetObject>& assetObject, bool replaceOnConflict)
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
            HYP_LOG(Assets, Verbose, "AssetObject '{}' belongs to package {} and will be removed from it before being added to package {}",
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

    FilePath packageDir;

    bool isLoading = false;
    bool isPackageSavedInFilesystem = false;
    bool shouldSaveAsset = false;

    // assigned if we removed an old one
    Handle<AssetObject> existingAssetObject;

    { // lock scope (unique)
        TUniqueLock guard(m_mutex);

        packageDir = m_packageDir;

        isLoading = IsLoading();
        isPackageSavedInFilesystem = !IsTransient() && IsSaved_Internal();
        shouldSaveAsset = ShouldSavePackageOnChanged(*this) && !isLoading;

        // if no name is provided for the asset, generate one
        if (!assetObject->m_name.IsValid())
        {
            assetObject->m_name = GetUniqueAssetName_Internal(assetObject->InstanceClass()->GetName());
        }
        
        assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);
        assetObject->m_package = WeakHandleFromThis();

        if (!shouldSaveAsset && !isPackageSavedInFilesystem && !isLoading)
        {
            assetObject->SetIsTransientByProxy(true);
        }

        // check for existing and replace if requested
        auto existingAssetObjectIt = m_assetObjects.Find(assetObject->GetName());

        if (existingAssetObjectIt != m_assetObjects.End())
        {
            if (*existingAssetObjectIt == assetObject)
            {
                return {};
            }

            if (!replaceOnConflict)
            {
                return HYP_MAKE_ERROR(Error, "AssetObject with name '{}' already exists in package '{}'", assetObject->GetName(), BuildPackagePath());
            }

            // remove existing
            existingAssetObject = *existingAssetObjectIt;

            existingAssetObject->OnUnloaded();

            existingAssetObject->m_package.Reset();
            existingAssetObject->m_assetPath = {};

            assetObject->m_manifestPath = existingAssetObject->m_manifestPath;
            existingAssetObject->m_manifestPath = FilePath();

            existingAssetObject->MarkDirty();

            m_assetObjects.Erase(existingAssetObjectIt);

            HYP_LOG(Assets, Verbose, "AssetObject with name '{}' already exists in package '{}'. Replacing it with the new one.",
                assetObject->GetName(), BuildPackagePath());

            // continue with adding the new one below
        }
        
        m_assetObjects.Insert(assetObject);

        if (isLoading)
        {
            assetObject->SetIsTransientByProxy(false);
        }
        else if (!shouldSaveAsset)
        {
            assetObject->MarkDirty();
            MarkDirty();
        }
    } // end lock scope
    
    InitObject(assetObject);

    // notify asset object removed if that's the case
    if (existingAssetObject.IsValid())
    {
        OnAssetObjectRemoved(existingAssetObject, true);

        AssetPackage* parentPackage = m_parentPackage;

        while (parentPackage != nullptr)
        {
            parentPackage->OnAssetObjectRemoved(existingAssetObject, false);
            parentPackage = parentPackage->GetParentPackage();
        }

        HYP_LOG(Assets, Verbose, "Removed {} '{}' from package '{}'",
            existingAssetObject->InstanceClass()->GetName(),
            existingAssetObject->GetName(),
            BuildPackagePath());
    }

    HYP_LOG(Assets, Verbose, "Added {} '{}' to package '{}' (thread: {})",
        assetObject->InstanceClass()->GetName(),
        assetObject->GetName(),
        BuildPackagePath(),
        CurrentThreadId().GetName());

    if (shouldSaveAsset)
    {
        AssertDebug(!assetObject->IsTransient() || assetObject->IsTransientByProxy());
            
        const FilePath newManifestFilepath = packageDir / *assetObject->GetName() + ".json";

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

    AssetPackage* parentPackage = m_parentPackage;

    while (parentPackage != nullptr)
    {
        parentPackage->OnAssetObjectAdded(assetObject, false);
        parentPackage = parentPackage->GetParentPackage();
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

        assetObject->OnUnloaded();

        assetObject->m_package.Reset();
        assetObject->m_assetPath = {};

        m_assetObjects.Erase(it);

        MarkDirty();
        assetObject->MarkDirty();
    }

    OnAssetObjectRemoved(assetObject, true);

    AssetPackage* parentPackage = m_parentPackage;

    while (parentPackage != nullptr)
    {
        parentPackage->OnAssetObjectRemoved(assetObject, false);
        parentPackage = parentPackage->GetParentPackage();
    }

    HYP_LOG(Assets, Verbose, "Removed {} '{}' from package '{}'",
        assetObject->InstanceClass()->GetName(),
        assetObject->GetName(),
        BuildPackagePath());

    /// TODO: remove the file

    return {};
}

Handle<AssetObject> AssetPackage::GetAssetObject(UTF8StringView assetName, bool attemptLoading)
{
    if (!assetName)
    {
        return {};
    }

    { // check for existing asset
        TSharedLock guard(m_mutex);

        auto it = m_assetObjects.FindAs(StringHash(assetName));

        if (it != m_assetObjects.End())
        {
            return *it;
        }
    }

    if (attemptLoading)
    {
        m_loadedMutex.Lock();

        if (IsLoading())
        {
            if (m_loadingThreadId == CurrentThreadId())
            {
                m_loadedMutex.Unlock();

                // currently loading on this thread; force load this asset immediately.
                // can happen if we are loading assets from LoadPackageFromManifest() one by one,
                // and one asset's loading procedure requires this asset to be loaded.

                // @TODO Combine this code with the code in LoadPackageFromManifest that does this.

                Handle<AssetObject> assetObject;

                FilePath manifestPath = m_packageDir / String(assetName) + ".json";
                FileByteReader stream { manifestPath };

                JSON::Object manifestData;

                if (Result readManifestResult = ReadManifest(stream, manifestPath, manifestData); readManifestResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to read asset manifest: {}", readManifestResult.GetError().GetMessage());

                    return Handle<AssetObject>::empty;
                }
                
                Result loadResult = AssetObject::Load(manifestData, assetObject);

                if (loadResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to load asset: {}", loadResult.GetError().GetMessage());

                    return Handle<AssetObject>::empty;
                }

                assetObject->m_package = WeakHandleFromThis();
                assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

                { // put the asset into the package
                    TUniqueLock packageLock(m_mutex);

                    auto insertResult = m_assetObjects.Insert(assetObject);
                    AssertDebug(insertResult.second, "Asset already added to package while loading?");
                }

                InitObject(assetObject);
                
                assetObject->OnLoaded();

                OnAssetObjectAdded(assetObject, true);
                
                AssetPackage* parentPackage = m_parentPackage;

                while (parentPackage != nullptr)
                {
                    parentPackage->OnAssetObjectAdded(assetObject, false);
                    parentPackage = parentPackage->GetParentPackage();
                }

                return assetObject;
            }

            // wait for other thread to finish loading it
            while (IsLoading())
            {
                m_loadedCV.Wait(m_loadedMutex);
            }
        }

        m_loadedMutex.Unlock();
    }

    return Handle<AssetObject>::empty;
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

    HashSet<StringHash> currentAssetNames;
    ForEachAssetObject([&](const Handle<AssetObject>& asset)
        {
            currentAssetNames.Add(asset->GetName());

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

        String desiredName = asset->GetName().LookupString();

        const bool renameOnNameClash = ShouldUniquifyAssetNames(*this);

        // check if name is already taken in destination package
        if (renameOnNameClash && currentAssetNames.Contains(StringHash(desiredName)))
        {
            Name uniqueName = GetUniqueAssetName(CreateNameFromDynamicString(desiredName));

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
                Handle<AssetObject> existingAssetObject = GetAssetObject(desiredName, /* attemptLoading */ false);

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

        if (Result addResult = AddAssetObject(asset, /* replaceOnConflict */ false); addResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to add asset '{}' to destination package '{}' during merge: {}", asset->GetName(), GetName(), addResult.GetError().GetMessage());
        }
    }

    Handle<AssetPackage> strongThis = MakeStrongRef(this);

    // needed for GetPackageFromPath() / GetPackage().
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

            Handle<AssetPackage> dest = registry->GetPackage(strongThis, sub->GetName().LookupString(), /* createIfNotExist */ true);
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

    AssetPackage* parentPackage = m_parentPackage;

    if (!parentPackage)
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

    AssetPackage* parentPackage = m_parentPackage;

    while (parentPackage != nullptr)
    {
        chain.PushBack(parentPackage->m_name);

        parentPackage = parentPackage->GetParentPackage();
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
    
    if (IsPackageInList(name, PredefinedTransientPackages))
    {
        m_flags |= AssetPackageFlags::Hidden | AssetPackageFlags::Transient;
    }
    else
    {
        const char* str = name.LookupString();

        if (str[0] == '$')
        {
            m_flags |= AssetPackageFlags::Hidden;
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
    if (AssetPackage* parentPackage = m_parentPackage)
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

bool AssetPackage::HasAssetWithName(StringHash assetName) const
{
    HYP_SCOPE;

    if (!assetName)
    {
        return false;
    }

    TSharedLock guard(m_mutex);
    return m_assetObjects.Contains(assetName);
}

Name AssetPackage::GetUniqueAssetName(Name baseName) const
{
    HYP_SCOPE;

    if (!baseName)
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

    Handle<AssetRegistry> registry;
    bool skipSavingThisPackage = false;
    
    { // check what / if we should skip
        TSharedLock lock(m_mutex);

        if (IsTransient())
        {
            return HYP_MAKE_ERROR(Error, "Cannot save transient AssetPackage '{}'", m_name);
        }

        registry = m_registry.Lock();

        if (!registry)
        {
            return HYP_MAKE_ERROR(Error, "AssetPackage '{}' does not have a valid AssetRegistry", m_name);
        }

        // If saveEvenIfNotDirty is false (default), check if we should save
        //  - if it has been saved before, we need to check if is dirty
        //    and additionally check if any individual asset objects are dirty.
        if (!saveEvenIfNotDirty && IsSaved_Internal())
        {
            if (!IsDirty())
            {
                if (HasDirtyAssetObjects())
                {
                    lock.Reset();

                    MarkDirty();
                }
            }

            if (!IsDirty())
            {
                // Already saved and not marked dirty; return ok
                skipSavingThisPackage = true;
            }
        }
    }
    
    TUniqueLock lock(m_mutex);

    FilePath packageDir;

    // Build package save dir. `outputDirectory` /may/ just be a base path, where we'll append the package path to it
    const String packagePath = BuildPackagePath();

    Array<String> outputParts = outputDirectory.Split('/', '\\');
    Array<String> packageParts = packagePath.Split('/', '\\');

    size_t packageStartIndex = 0;

    if (packageParts.Any() && outputParts.Any())
    {
        // Check if the last part of outputDirectory matches any part of packagePath
        const String& lastOutputPart = outputParts.Back();

        for (size_t i = 0; i < packageParts.Size(); ++i)
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
        for (size_t i = packageStartIndex; i < packageParts.Size(); ++i)
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
        m_lastSavedTimestamp = Time::Now();
    }

    Name packageName = m_name;

    AssetPackageSet subpackages = m_subpackages;
    AssetObjectSet assetObjects = m_assetObjects;

    const bool shouldSaveAssets = !skipSavingThisPackage && !IsTransient() && IsSaved_Internal();

    lock.Reset();

    // even if skipSaving is true, we need to iterate over subpackages as
    // they may have individual asset objects that are dirty
    for (const Handle<AssetPackage>& subpackage : subpackages)
    {
        if (subpackage->IsTransient())
        {
            continue;
        }

        Result result = subpackage->Save(packageDir / *subpackage->GetName(), saveEvenIfNotDirty);

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save subpackage '{}' of package '{}': {}",
                subpackage->GetName(), packageName, result.GetError().GetMessage());
        }
    }

    if (!skipSavingThisPackage && !IsTransient() && IsSaved_Internal())
    {
        for (const Handle<AssetObject>& assetObject : assetObjects)
        {
            // If TRANSIENT (not BY PROXY), skip saving this asset
            if ((assetObject->GetAssetFlags() & (AssetObjectFlags::Transient | AssetObjectFlags::TransientByProxy)) == AssetObjectFlags::Transient)
            {
                continue;
            }

            if (Result saveAssetResult = assetObject->Save(packageDir / *assetObject->GetName() + ".json"); saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}",
                    assetObject->GetName(), packageName, saveAssetResult.GetError().GetMessage());

                continue;
            }

            assetObject->SetIsTransientByProxy(false);
        }

        // unset dirty state
        AtomicBitAnd(&m_stateFlags, ~SF_Dirty);
    }

    return {};
}

Result AssetPackage::SaveManifest(ByteWriter& stream) const
{
    HYP_SCOPE;

    JSON::Object manifestJson;
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

            HYP_LOG(Assets, Verbose, "Added dependency to package '{}': {}", m_name, dependency.ToString());
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

                    HYP_LOG(Assets, Verbose, "Pruning asset object '{}' from package '{}'", assetObject->GetName(), m_name);

                    assetObject->OnUnloaded();

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

                AssetPackage* parentPackage = m_parentPackage;

                while (parentPackage != nullptr)
                {
                    parentPackage->OnAssetObjectRemoved(assetObject, false);
                    parentPackage = parentPackage->GetParentPackage();
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

            AssetPackage* parentPackage = m_parentPackage;

            while (parentPackage != nullptr)
            {
                parentPackage->OnSubpackageRemoved(subpackage);

                parentPackage = parentPackage->GetParentPackage();
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

    // we assume m_mutex is locked here.

    constexpr int MaxRecursionDepth = 32;
    static thread_local int s_recursionDepth = 0;

    HYP_DEFER({ --s_recursionDepth; });

    if (s_recursionDepth++ > MaxRecursionDepth)
    {
        HYP_LOG(Assets, Error, "Max recursion depth reached in AssetPackage::MarkDirty for package '{}'", m_name);

        return;
    }

    if (!(AtomicBitOr(&m_stateFlags, SF_Dirty) & SF_Dirty))
    {
        if (AssetPackage* parentPackage = m_parentPackage)
        {
            TSharedLock parentPackageLock(parentPackage->m_mutex);
            parentPackage->MarkDirty();
        }
    }
}

void AssetPackage::WaitUntilLoaded()
{
    Mutex::Guard guard(m_loadedMutex);

    AssertDebug(m_loadingThreadId != CurrentThreadId());

    if (m_loadingThreadId == CurrentThreadId())
    {
        return;
    }

    while (IsLoading())
    {
        m_loadedCV.Wait(m_loadedMutex);
    }
}

void AssetPackage::SignalLoaded()
{
    Mutex::Guard guard(m_loadedMutex);

    AssertDebug(m_loadingThreadId == CurrentThreadId());

    if (m_loadingThreadId != CurrentThreadId())
    {
        return;
    }

    m_loadingThreadId = ThreadId::Invalid();

    m_loadedCV.NotifyAll();
}

#pragma endregion AssetPackage

#pragma region AssetRegistry

AssetRegistry::AssetRegistry()
    : AssetRegistry("Packages")
{
}

AssetRegistry::AssetRegistry(const String& rootPath)
    : m_rootPath(rootPath),
      m_scheduler(new Scheduler(s_assetRegistryThread)),
      m_pruneTimer { 30.0 },        // every 30 seconds
      m_pruneTaskBatch(nullptr),
      m_saveBlobCacheTimer { 5.0 }, // every 5 seconds
      m_saveBlobCacheBatch(nullptr),
      m_blobStorage(nullptr)
{
}

AssetRegistry::~AssetRegistry()
{
    if (m_pruneTaskBatch != nullptr)
    {
        if (!m_pruneTaskBatch->IsCompleted())
        {
            HYP_LOG(Assets, Info, "Waiting for prune task batch to complete before destroying AssetRegistry...");
            m_pruneTaskBatch->AwaitCompletion();
        }

        delete m_pruneTaskBatch;
        m_pruneTaskBatch = nullptr;
    }

    if (m_saveBlobCacheBatch != nullptr)
    {
        if (!m_saveBlobCacheBatch->IsCompleted())
        {
            HYP_LOG(Assets, Info, "Waiting for blob cache to finish saving");
            m_saveBlobCacheBatch->AwaitCompletion();
        }

        delete m_saveBlobCacheBatch;
        m_saveBlobCacheBatch = nullptr;
    }

    if (m_blobStorage != nullptr)
    {
        m_blobStorage->Release();
        m_blobStorage = nullptr;
    }

    delete m_scheduler;
}

void AssetRegistry::Initialize()
{
    HYP_SCOPE;

    AssertOnThread(g_mainThread);

    InitBlobStorage();
    
    // in-memory package store always exists
    Handle<AssetPackage> memoryPackage = GetPackageFromPath("$Memory", true);

    // editor specific packages
#if HYP_EDITOR
    // load Engine package before importing others (they may reference stuff in this package)
    Handle<AssetPackage> enginePackage = GetPackageFromPath("Engine", true);
    Assert(enginePackage.IsValid());
    enginePackage->Save(GetLibraryDirectory());

    LoadPackagesAsync(/* loadSubpackages */ false);

    Handle<AssetPackage> tempPackage = GetPackageFromPath("$Temp", true);
    Assert(tempPackage.IsValid());
    tempPackage->Save(CoreApi::GetExecutablePath());
    
    // Add transient package for imported assets in editor mode
    Handle<AssetPackage> importsPackage = GetPackageFromPath("$Import", true);
    Assert(importsPackage.IsValid());

    m_onEngineShutdown = g_engineDriver->GetDelegates().OnShutdown.Bind([this, weakThis = MakeWeakRef(this)]()
        {
            Handle<AssetRegistry> strongThis = weakThis.Lock();
            if (!strongThis.IsValid())
                return;

            AssetPackageSet packages;

            { // grab packages after prune task batch is complete (if applicable)
                TUniqueLock lock(m_mutex);

                if (m_pruneTaskBatch != nullptr)
                {
                    if (!m_pruneTaskBatch->IsCompleted())
                    {
                        HYP_LOG(Assets, Warning, "Waiting for prune task batch to complete.");
                        m_pruneTaskBatch->AwaitCompletion();
                    }

                    delete m_pruneTaskBatch;
                    m_pruneTaskBatch = nullptr;
                }

                packages = std::move(m_packages);
            }

            packages.Clear();

            if (m_blobStorage != nullptr)
            {
                Result result = m_blobStorage->SaveTOC();

                if (result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save blob storage table of contents! Error message was: {}", result.GetError().GetMessage());
                }

                result = m_blobStorage->SaveManifest();

                if (result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save blob storage manifest! Error message was: {}", result.GetError().GetMessage());
                }
            }
        });
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

void AssetRegistry::SaveBlobCache(bool async)
{
    auto DoSaveBlobCache = [this, weakThis = MakeWeakRef(this)]()
    {
        Handle<AssetRegistry> registry = weakThis.Lock();

        if (!registry.IsValid())
            return;

        if (m_blobStorage != nullptr)
        {
            Result result = m_blobStorage->SaveIfDirty();

            if (result.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save blob storage - error message was: {}", result.GetError().GetMessage());
            }
        }
    };

    if (async)
    {
        if (m_saveBlobCacheBatch != nullptr)
        {
            if (!m_saveBlobCacheBatch->IsCompleted())
            {
                HYP_LOG(Assets, Warning, "Skipping saving blob cache - async task already processing");

                return;
            }

            m_saveBlobCacheBatch->ResetState();
        }
        else
        {
            m_saveBlobCacheBatch = new TaskBatch;
            m_saveBlobCacheBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_BACKGROUND);
        }

        m_saveBlobCacheBatch->AddTask(DoSaveBlobCache);

        TaskSystem::GetInstance().EnqueueBatch(m_saveBlobCacheBatch);
    }
    else
    {
        DoSaveBlobCache();
    }
}

BlobStorage& AssetRegistry::GetBlobStorage()
{
    Assert(m_blobStorage != nullptr);

    return *m_blobStorage;
}

void AssetRegistry::InitBlobStorage()
{
    if (m_blobStorage != nullptr)
    {
        return;
    }

    const FilePath& s_blobStorageLocation = GetCacheDirectory();
    const uint64 s_blobStoragePageSize = CoreApi::GetGlobalConfig().Get("App.Cache.PageSize")
        .ToUInt64(/* defaultValue */ BlobStorage::DefaultPageSize);
    
    m_blobStorage = new BlobStorage(s_blobStorageLocation, s_blobStoragePageSize);
}

void AssetRegistry::Update()
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

#if HYP_EDITOR
    if (!m_pruneTimer.Waiting())
    {
        m_pruneTimer.NextTick();

        PruneTransientPackages();
    }

    if (!m_saveBlobCacheTimer.Waiting())
    {
        m_saveBlobCacheTimer.NextTick();

        SaveBlobCache(/* async */ true);
    }
#endif

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

void AssetRegistry::LoadPackagesAsync(bool loadSubpackages)
{
    HYP_SCOPE;

    Array<FilePath> dirs;
    dirs.PushBack(GetLibraryDirectory());

#if HYP_EDITOR
    dirs.PushBack(GetProjectsDirectory());
#endif

    TaskSystem::GetInstance().Enqueue([this, weakThis = WeakHandleFromThis(), dirs, loadSubpackages]()
        {
            HYP_NAMED_SCOPE("AssetRegistry::LoadPackagesAsync");

            Handle<AssetRegistry> registry = weakThis.Lock();
            if (!registry)
            {
                HYP_LOG(Assets, Error, "AssetRegistry is no longer valid, cannot load packages");
                return;
            }

            AssetPackageSet rootPackages;

            Proc<void(const FilePath& dir)> IterateDirectory;

            IterateDirectory = [&](const FilePath& dir)
            {
                if (!dir.Exists() || !dir.IsDirectory())
                {
                    // nothing to load if it doesnt exist
                    return;
                }

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
                    if (subdirectory.Basename() == "Engine")
                    {
                        continue; // skip Engine folder, since we load it on the main thread
                    }

                    // recursively iterate subdirectories
                    IterateDirectory(subdirectory);
                }
            };

            for (const FilePath& base : dirs)
            {
                IterateDirectory(base);
            }
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

    Proc<void(Handle<AssetPackage>)> InitializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    InitializePackage = [this, &InitializePackage](const Handle<AssetPackage>& package)
    {
        Assert(package.IsValid());

        package->m_registry = MakeWeakRef(this);

        InitObject(package);

        OnPackageAdded(package);

        for (const Handle<AssetPackage>& subpackage : package->m_subpackages)
        {
            subpackage->m_parentPackage = package;
            subpackage->m_flags |= package->m_flags;

            InitializePackage(subpackage);
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
        InitializePackage(package);
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

        /// TODO: Refactor to use `MergePackage` when AssetRegistry is not needed for `GetPackage()`
        Proc<void(const Handle<AssetPackage>&, const Handle<AssetPackage>&)> MergeInto;

        MergeInto = [this, &MergeInto](const Handle<AssetPackage>& dest, const Handle<AssetPackage>& src)
        {
            if (!dest.IsValid() || !src.IsValid())
            {
                return;
            }

            HashSet<StringHash> destAssetNames;
            dest->ForEachAssetObject([&](const Handle<AssetObject>& asset)
                {
                    destAssetNames.Add(asset->GetName());

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

                String desiredName = asset->GetName().LookupString();

                const bool renameOnNameClash = ShouldUniquifyAssetNames(*dest);

                // check if name is already taken in destination package
                if (renameOnNameClash && destAssetNames.Contains(StringHash(desiredName)))
                {
                    Name uniqueName = dest->GetUniqueAssetName(CreateNameFromDynamicString(desiredName));

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
                    if (destAssetNames.Contains(StringHash(desiredName)))
                    {
                        Handle<AssetObject> existingAssetObject = dest->GetAssetObject(desiredName, /* attemptLoading */ false);

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
                Result addResult = dest->AddAssetObject(asset, /* replaceOnConflict */ false);
                if (addResult.HasError())
                {
                    //HYP_LOG(Assets, Error, "Failed to add asset '{}' to destination package: {}",
                     //   asset->GetName(), addResult.GetError().GetMessage());
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

                Handle<AssetPackage> destSubpackage = GetPackage(dest, sub->GetName().LookupString(), /* createIfNotExist */ true);
                Assert(destSubpackage != nullptr);

                MergeInto(destSubpackage, sub);
            }
        };

        MergeInto(existing, package);

        // update reference
        // package = existing;

        return {};
    }

    Handle<AssetPackage> newParentPackage;
    if (AssetPackage* prevParentPackage = package->GetParentPackage())
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

        AssetPackage* parentPackage = nullptr;

        if (package->m_parentPackage != nullptr)
        {
            parentPackage = package->m_parentPackage;

            if (parentPackage != nullptr)
            {
                auto it = parentPackage->m_subpackages.Find(package->GetName());
                Assert(it != parentPackage->m_subpackages.End());

                parentPackage->m_subpackages.Erase(it);
                
                lock.Reset(parentPackage->m_mutex);

                parentPackage->OnSubpackageRemoved(strongPackage);

                removed = true;

                parentPackage->MarkDirty();
            }

            package->m_parentPackage = nullptr;
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

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(
    const UTF8StringView& path, bool createIfNotExist, bool requireLoaded)
{
    HYP_SCOPE;

    return GetPackageFromPath_Internal(
        path, createIfNotExist, requireLoaded);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(
    const UTF8StringView& path, bool createIfNotExist, bool requireLoaded)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::Char32('/') || *it == utf::Char32('\\'))
        {
            currentPackage = GetPackage(currentPackage, currentString, createIfNotExist, requireLoaded);

            currentString.Clear();

            if (!currentPackage)
            {
                return Handle<AssetPackage>::Null();
            }

            continue;
        }

        currentString.Append(*it);
    }
    
    // if there is any remaining string, get / create the subpackage
    if (!currentPackage.IsValid() || currentString.Any())
    {
        currentPackage = GetPackage(currentPackage, currentString, createIfNotExist, requireLoaded);
    }

    return currentPackage;
}

Handle<AssetObject> AssetRegistry::GetAssetFromPath(const UTF8StringView& path, bool attemptLoading) const
{
    HYP_SCOPE;

    String assetName;

    Handle<AssetObject> asset = const_cast<AssetRegistry*>(this)->GetAssetFromPath_Internal(path, assetName, attemptLoading);

    if (asset.IsValid())
    {
        return asset;
    }

    HYP_LOG(Assets, Error, "Could not get asset at path '{}'", path);

    return Handle<AssetObject>::empty;
}

Handle<AssetObject> AssetRegistry::GetAssetFromPath_Internal(
    const UTF8StringView& path, String& outAssetName, bool attemptLoading)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::Char32('/') || *it == utf::Char32('\\'))
        {
            currentPackage = GetPackage(currentPackage, currentString, /* createIfNotExist */ false, /* requireLoaded */ false);

            currentString.Clear();

            if (!currentPackage)
            {
                return Handle<AssetObject>::empty;
            }

            continue;
        }

        currentString.Append(*it);
    }

    outAssetName = std::move(currentString);

    if (currentPackage.IsValid() && outAssetName.Any())
    {
        return currentPackage->GetAssetObject(outAssetName, attemptLoading);
    }

    return Handle<AssetObject>::empty;
}

Handle<AssetPackage> AssetRegistry::GetPackage(
    const Handle<AssetPackage>& parentPackage,
    const UTF8StringView& subpackageName,
    bool createIfNotExist,
    bool requireLoaded)
{
    HYP_SCOPE;

    Handle<AssetPackage> pkg;

    const bool canLoadPackage = !IsPackageInList(subpackageName, PredefinedTransientPackages);

    bool isSubpackageSaved = false;

    if (!parentPackage) // top-level package
    {
        TUniqueLock guard(m_mutex);

        auto packageIt = m_packages.Find(Name(StringHash(subpackageName)));

        if (packageIt != m_packages.End())
        {
            pkg = *packageIt;

            if (requireLoaded && pkg->IsLoading())
            {
                // wait until other thread is finished loading it
                // if no other thread is loading it, this will return right away
                pkg->WaitUntilLoaded();
            }
        }
        else
        {
            if (canLoadPackage)
            {
                // Try loading from manifest path, if it exists.
                FilePath subpackageDir = GetLibraryDirectory() / String(subpackageName);
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

                            OnPackageAdded(pkg);
                        }
                    }
                }
            }
            
            // still not valid and createIfNotExist is true? create it.
            if (!pkg.IsValid() && createIfNotExist)
            {
                pkg = MakeHandle<AssetPackage>(CreateNameFromDynamicString(subpackageName));
                pkg->m_registry = WeakHandleFromThis();

                InitObject(pkg);

                pkg->MarkDirty();

                m_packages.Insert(pkg);

                OnPackageAdded(pkg);
            }
        }

        return pkg;
    }
    
    TUniqueLock guard(parentPackage->m_mutex);

    auto packageIt = parentPackage->m_subpackages.Find(subpackageName);

    if (packageIt != parentPackage->m_subpackages.End())
    {
        pkg = *packageIt;

        if (requireLoaded && pkg->IsLoading())
        {
            pkg->WaitUntilLoaded();
        }
    }
    else
    {
        // Try load from manifest path
        FilePath subpackageDir = parentPackage->m_packageDir / String(subpackageName);
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
            pkg = MakeHandle<AssetPackage>(CreateNameFromDynamicString(subpackageName));
            pkg->m_registry = WeakHandleFromThis();
            pkg->m_parentPackage = parentPackage;
            pkg->m_flags |= parentPackage->m_flags;
            pkg->m_stateFlags = AssetPackage::SF_Dirty;

            // If parent package exists on disk, save this package:
            if (!parentPackage->IsTransient() && parentPackage->IsSaved_Internal())
            {
                FilePath subpackageDir = parentPackage->m_packageDir / String(subpackageName);

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

            parentPackage->MarkDirty();
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
        Handle<AssetPackage> existingSubpackage = GetPackage(package, subpackage->GetName().LookupString(), /* createIfNotExist */ true);
        Assert(existingSubpackage != nullptr);

        if (existingSubpackage != subpackage)
        {
            HYP_LOG(Assets, Verbose, "Subpackage with name '{}' already exists in package '{}', skipping loaded subpackage from '{}'",
                subpackage->GetName(), package->BuildPackagePath(), manifestPath);
        }
    }
}

TResult<Handle<AssetPackage>> AssetRegistry::LoadPackageFromManifest(
    const FilePath& manifestPath, bool loadSubpackages, bool forceLoad)
{
    HYP_SCOPE;

    HYP_LOG(Assets, Verbose, "Loading package from manifest path: {}", manifestPath);

    Handle<AssetPackage> outPackage;

    if (!manifestPath.Exists() || manifestPath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Manifest file '{}' does not exist or is not a file", manifestPath);
    }

    const FilePath dir = manifestPath.BasePath();

    FileByteReader stream { manifestPath };

    if (stream.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file '{}'", manifestPath);
    }

    String str = String(stream.Read().ToByteView());

    JSON::ParseResult parseResult = JSON::Parse(str);

    stream.Close();
    str = {};

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
        if (Handle<AssetPackage> existingPackage = GetPackageFromPath(packagePath, /* createIfNotExist */ false, /* requireLoaded */ false); existingPackage.IsValid())
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
            parentPackage = GetPackageFromPath(parentPackagePathString, /* createIfNotExist */ false, /* requireLoaded */ false);

            if (!parentPackage)
            {
                const String relativePath = AssetPath::MakeRelativePath(AssetPath(packagePath), AssetPath(parentPackagePathString));

                // make filepath for parent package manifest
                const FilePath parentManifestPath = dir / relativePath / "PackageManifest.json";

                HYP_LOG(Assets, Verbose, "Loading parent package '{}' for package at '{}' from manifest '{}'", parentPackagePathString, packagePath, parentManifestPath);

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
    outPackage->m_lastSavedTimestamp = manifestPath.LastModifiedTimestamp();

    // start out in loading state so other threads requesting this package will
    // have to wait for us, rather than trying to load repeatedly
    bool loadingStateCleared = false;
    outPackage->m_stateFlags = AssetPackage::SF_Loading;
    outPackage->m_loadingThreadId = CurrentThreadId();

    if (parentPackage)
    {
        outPackage->m_flags |= parentPackage->m_flags;

        TUniqueLock parentPackageLock(parentPackage->m_mutex);
        parentPackage->m_subpackages.Insert(outPackage); // NOTE do not broadcast change yet
    }
    else
    {
        TUniqueLock registryLock(m_mutex);
        m_packages.Insert(outPackage); // same as above, don't broadcast yet
    }

    HYP_DEFER({
        if (outPackage && !loadingStateCleared)
        {
            AtomicBitAnd(&outPackage->m_stateFlags, ~AssetPackage::SF_Loading);
            outPackage->SignalLoaded(); // just to wake up other waiting threads so we don't deadlock on error
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

        HYP_LOG(Assets, Verbose, "Loading dependency package '{}' for package '{}'", dependencyPath, outPackage->GetName());

        Handle<AssetPackage> dependencyPackage = GetPackageFromPath(dependencyPath.ToString(), /* createIfNotExist */ false);

        if (dependencyPackage != nullptr)
        {
            HYP_LOG(Assets, Verbose, "Dependency package '{}' already loaded!", dependencyPath);
            continue;
        }

        // Dependency package doesn't exist yet, try to load it from filesystem
        const String relativePath = AssetPath::MakeRelativePath(AssetPath(outPackage->BuildPackagePath()), dependencyPath);
        const FilePath dependencyManifestPath = GetLibraryDirectory() / FilePath::Relative(dir / relativePath / "PackageManifest.json", GetLibraryDirectory());

        if (dependencyManifestPath.Exists() && !dependencyManifestPath.IsDirectory())
        {
            HYP_LOG(Assets, Verbose, "Loading dependency package '{}' from manifest '{}'", dependencyPath, dependencyManifestPath);

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

    for (auto iter = dir.OpenDirectory(); iter.HasNext(); iter.Advance())
    {
        if (iter.CurrentIsDirectory())
        {
            continue;
        }

        FilePath curr = iter.Current();
        
        if (curr.GetExtension() != "json")
        {
            continue;
        }

        if (curr.Basename() == "PackageManifest.json")
        {
            // Skip the package manifest itself
            continue;
        }

        assetFiles.PushBack(curr);
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

            FileByteReader stream { entry };

            // here we want to read the manifest to get the `Name` property of the asset,
            // so we can check if the asset is already loaded.
            // the reason we do this is because:
            //   AssetObject::Load() may inadvertently trigger loads of other assets
            //   that are in the same package we're currently loading from,
            //   and we want to reduce the risk of double-loading an asset.
            
            JSON::Object manifestData;
            if (Result readManifestResult = ReadManifest(stream, entry, manifestData); readManifestResult.HasError())
            {
                return Error(readManifestResult.GetError());
            }

            String assetName = manifestData["Name"].ToString();
            if (assetName.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Asset manifest at path '{}' has invalid asset name property", entry);
            }

            // check if we already have it
            {
                StringHash assetNameHash = StringHash(assetName);

                TSharedLock lock(outPackage->m_mutex);

                auto existingAssetIt = outPackage->m_assetObjects.Find(assetNameHash);

                if (existingAssetIt != outPackage->m_assetObjects.End())
                {
                    HYP_LOG(Assets, Verbose, "Asset {} in package {} already loaded by external forces, skipping",
                        assetName, outPackage->BuildPackagePath());

                    continue;
                }
            }

            Handle<AssetObject> assetObject;

            if (Result loadAssetResult = AssetObject::Load(manifestData, assetObject); loadAssetResult.HasError())
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

            if (Result addAssetResult = outPackage->AddAssetObject(assetObject, /* replaceOnConflict */ false); addAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to add asset to package '{}': {}", outPackage->GetName(), addAssetResult.GetError().GetMessage());

                continue;
            }
            
            assetObject->OnLoaded();
        }
    }

    // Load subpackages after assets (if requested)
    // Each subpackage will be loaded with loadSubpackages=true to recursively load their children
    if (loadSubpackages)
    {
        for (auto dirIter = dir.OpenDirectory(); dirIter.HasNext(); dirIter.Advance())
        {
            if (!dirIter.CurrentIsDirectory())
            {
                continue;
            }

            const FilePath subdirectory = dirIter.Current();

            for (auto subdirIter = subdirectory.OpenDirectory(); subdirIter.HasNext(); subdirIter.Advance())
            {
                if (subdirIter.CurrentIsDirectory())
                {
                    continue;
                }

                FilePath entry = subdirIter.Current();

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

    InitObject(outPackage);

    loadingStateCleared = true;
    AtomicBitAnd(&outPackage->m_stateFlags, ~AssetPackage::SF_Loading);
    outPackage->SignalLoaded();

    if (parentPackage != nullptr)
    {
        parentPackage->OnSubpackageAdded(outPackage);
    }
    else // top-level package
    {
        OnPackageAdded(outPackage);
    }

    return outPackage;
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

Result AssetRegistry::RegisterAsset(
    const UTF8StringView& path,
    const Handle<AssetObject>& assetObject,
    AddAssetConflictMode conflictMode)
{
    HYP_SCOPE;

    if (!assetObject.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    Array<String> pathStringSplit = String(path).Split('/', '\\');
    String pathString = String::Join(pathStringSplit, '/');

    Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(pathString, /* createIfNotExist */ true, /* requireLoaded */ false);
    AssertDebug(assetPackage.IsValid());

    // check if it is already in that package
    if (assetObject->m_package.GetUnsafe() == assetPackage.Get())
    {
        return {}; // ok
    }

    if (conflictMode == AddAssetConflictMode::Default)
    {
        if (ShouldUniquifyAssetNames(*assetPackage))
        {
            conflictMode = AddAssetConflictMode::GenerateNewName;
        }
        else
        {
            conflictMode = AddAssetConflictMode::ReplaceExisting;
        }
    }

    const String desiredName = assetObject->GetName().LookupString();

    if (assetPackage->HasAssetWithName(StringHash(desiredName)))
    {
        if (conflictMode == AddAssetConflictMode::GenerateNewName)
        {
            const Name uniqueName = assetPackage->GetUniqueAssetName(CreateNameFromDynamicString(desiredName));

            if (Result renameResult = assetObject->Rename(uniqueName); renameResult.HasError())
            {
                return renameResult;
            }
        }
        else if (conflictMode == AddAssetConflictMode::ReplaceExisting)
        {
            if (Handle<AssetObject> existingAssetObject = assetPackage->GetAssetObject(desiredName, /* attemptLoading */ false); existingAssetObject.IsValid())
            {
                // remove old asset and overwrite it
                if (Result removeResult = assetPackage->RemoveAssetObject(existingAssetObject); removeResult.HasError())
                {
                    return removeResult;
                }
            }
        }
        else if (conflictMode == AddAssetConflictMode::FailOnConflict)
        {
            // return error
            return HYP_MAKE_ERROR(Error, "Could not register, asset with name {} already exists at path {}",
                desiredName, path);
        }
    }

    return assetPackage->AddAssetObject(assetObject, /* replaceOnConflict */ false);
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

    Proc<void(const Handle<AssetPackage>&, const BoxedValue&)> Iterate;
    Iterate = [&](const Handle<AssetPackage>& inPackage, const BoxedValue& current) -> void
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
                HYP_LOG(Assets, Verbose, "Already visited {} with ID {}, skipping to avoid infinite recursion",
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

            size_t size = array.Size();

            for (size_t i = 0; i < size; ++i)
            {
                BoxedValue boxed;
                if (!array.GetElementAt(i, boxed))
                {
                    HYP_LOG(Assets, Warning, "Failed to get element at index {} of array of type {}", i, LookupTypeName(current.GetTypeId()));
                    continue;
                }

                Iterate(parentPackage, boxed);
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

                        Iterate(parentPackage, BoxedValue(componentRef));
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
                if (IsRelocatable(assetReference.GetAssetPath()))
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

            Iterate(parentPackage, memberData);
        }

        if (assetObject != nullptr)
        {
            if (forceRelocation || !assetObject->IsRegistered())
            {
                if (Result result = parentPackage->AddAssetObject(assetObject, /* replaceOnConflict */ false); result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to register asset '{}': {}", assetObject->GetName(), result.GetError().GetMessage());
                }
            }
        }
    };

    Handle<AssetPackage> rootPackage = GetPackageFromPath(packagePath, /* createIfNotExist */ true);
    Assert(rootPackage.IsValid());

    Iterate(rootPackage, target);
}

#pragma endregion AssetRegistry

} // namespace Hyperion
