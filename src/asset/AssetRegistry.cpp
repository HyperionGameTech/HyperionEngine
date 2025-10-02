/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/object/HypDataJSONHelpers.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypProperty.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/json/JSON.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>

#include <system/MessageBox.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

//! for debugging
static constexpr bool g_disableAssetUnload = false;

extern HYP_API const FilePath& GetResourceDirectory();

WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage)
{
    if (!assetPackage.IsValid())
    {
        return {};
    }

    return assetPackage->GetName();
}

WeakName AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject)
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
        const utf::u32char c = *it;

        if (!std::isalnum(int(c)) && c != '$')
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
    while (elements.FindAs(WeakName(*str)) != elements.End())
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
        if (utf::utf32Isalpha(it) || utf::utf32Isdigit(it))
        {
            friendlyNameStr.Append(it);
        }
    }

    return CreateNameFromDynamicString(StringUtil::ToPascalCase(friendlyNameStr, true));
}

#pragma region AssetPackage

AssetPackage::AssetPackage()
    : AssetPackage(Name::Invalid())
{
}

AssetPackage::AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags)
    : m_flags(flags)
{
    if (name.IsValid())
    {
        // If the name starts with a '$', it's a transient package
        const char* str = name.LookupString();

        if (str[0] == '$')
        {
            m_flags |= APF_TRANSIENT | APF_HIDDEN;
        }

        m_name = SanitizeName(name);
        m_friendlyName = CreateFriendlyName(name);
    }
}

void AssetPackage::Init()
{
    HYP_SCOPE;

    HypObjectBase::Init();

    Handle<AssetRegistry> registry = m_registry.Lock();
    Assert(registry.IsValid());

    Array<Handle<AssetObject>> assetObjects;
    Array<Handle<AssetPackage>> subpackages;

    HashSet<AssetObject*> assetObjectsToSave;

    bool isPackageSavedInFilesystem = false;

    {
        Mutex::Guard guard(m_mutex);

        isPackageSavedInFilesystem = !IsTransient() && m_packageDir.Length() != 0;

        assetObjects.Reserve(m_assetObjects.Size());
        subpackages.Reserve(m_subpackages.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            if (IsTransient())
            {
                // transient data isn't saved to disk so we have to keep it in memory
                assetObject->SetIsPersistentlyLoaded(true);
            }
            else if (isPackageSavedInFilesystem)
            {
                const FilePath newAssetFilepath = m_packageDir / *assetObject->GetName();

                if (assetObject->m_filepath != newAssetFilepath)
                {
                    assetObject->m_filepath = newAssetFilepath;

                    assetObjectsToSave.Insert(assetObject.Get());
                }
            }

            InitObject(assetObject);

            assetObjects.PushBack(assetObject);
        }

        for (const Handle<AssetPackage>& subpackage : m_subpackages)
        {
            InitObject(subpackage);

            OnSubpackageAdded(subpackage);

            subpackages.PushBack(subpackage);
        }
    }

    for (const Handle<AssetObject>& assetObject : assetObjects)
    {
        if (assetObjectsToSave.Contains(assetObject.Get()))
        {
            // save the asset in our package
            if (Result saveAssetResult = assetObject->Save(); saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());

                continue;
            }

            assetObject->SetIsPersistentlyLoaded(false);
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

void AssetPackage::SetAssetObjects(const AssetObjectSet& assetObjects)
{
    if (IsInitCalled())
    {
        AssetObjectSet previousAssetObjects;

        { // store so we can call OnAssetObjectRemoved outside of the lock
            Mutex::Guard guard(m_mutex);

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
    }

    Array<Handle<AssetObject>> newAssetObjects;
    HashSet<AssetObject*> assetObjectsToSave;

    bool isPackageSavedInFilesystem = false;

    {
        Mutex::Guard guard(m_mutex);

        isPackageSavedInFilesystem = !IsTransient() && m_packageDir.Length() != 0;

        m_assetObjects = assetObjects;

        newAssetObjects.Reserve(m_assetObjects.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            assetObject->m_package = WeakHandleFromThis();
            assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

            if (IsTransient())
            {
                // transient data isn't saved to disk so we have to keep it in memory
                assetObject->SetIsPersistentlyLoaded(true);
            }
            else if (isPackageSavedInFilesystem)
            {
                const FilePath newAssetFilepath = m_packageDir / *assetObject->GetName();

                if (assetObject->m_filepath != newAssetFilepath)
                {
                    assetObject->m_filepath = newAssetFilepath;

                    assetObjectsToSave.Insert(assetObject.Get());
                }
            }

            InitObject(assetObject);

            newAssetObjects.PushBack(assetObject);
        }
    }

    if (IsInitCalled())
    {
        for (const Handle<AssetObject>& assetObject : newAssetObjects)
        {
            if (assetObjectsToSave.Contains(assetObject.Get()))
            {
                // save the file in our package
                Result saveAssetResult = assetObject->Save();

                if (saveAssetResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());

                    continue;
                }

                assetObject->SetIsPersistentlyLoaded(false);
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
}

Result AssetPackage::AddAssetObject(const Handle<AssetObject>& assetObject)
{
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
        HYP_LOG(Assets, Warning, "AssetObject '{}' already belongs to another package!", assetObject->GetName());
    }

    assetObject->m_package = WeakHandleFromThis();
    assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

    bool isPackageSavedInFilesystem = false;

    // we save the asset to the filesystem if:
    // the package is saved to the filesystem (not transient, has a package dir)
    // AND the asset's new filepath would differ from the current one it has (or it has never been saved)
    bool doSaveAsset = false;

    {
        Mutex::Guard guard(m_mutex);

        isPackageSavedInFilesystem = !IsTransient() && m_packageDir.Length() != 0;

        // if no name is provided for the asset, generate one
        if (!assetObject->GetName().IsValid())
        {
            assetObject->m_name = GetUniqueAssetName_Internal(assetObject->InstanceClass()->GetName());
        }

        if (IsTransient())
        {
            // transient data isn't saved to disk so we have to keep it in memory
            assetObject->SetIsPersistentlyLoaded(true);
        }
        else if (isPackageSavedInFilesystem)
        {
            // set a filepath for the asset object to be saved at, based on our package's filepath.
            const FilePath newAssetFilepath = m_packageDir / *assetObject->GetName();

            if (assetObject->m_filepath != newAssetFilepath)
            {
                assetObject->m_filepath = newAssetFilepath;

                doSaveAsset = true; // asset path changed, we need to save
            }
        }

        auto existingAssetObjectIt = m_assetObjects.Find(assetObject->GetName());

        if (existingAssetObjectIt != m_assetObjects.End())
        {
            if (*existingAssetObjectIt != assetObject)
            {
                return HYP_MAKE_ERROR(Error, "AssetObject with name '{}' already exists in package '{}'", assetObject->GetName(), m_name);
            }

            // already exists and is the same object; fine
            return {};
        }

        m_assetObjects.Insert({ assetObject });
    }

    if (IsInitCalled())
    {
        InitObject(assetObject);

        if (doSaveAsset)
        {
            // save the file in our package
            Result saveAssetResult = assetObject->Save();

            if (saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());

                return HYP_MAKE_ERROR(Error, "Failed to save asset object '{}': {}", assetObject->GetName(), saveAssetResult.GetError().GetMessage());
            }

            assetObject->SetIsPersistentlyLoaded(false);
        }

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }

    return {};
}

Result AssetPackage::RemoveAssetObject(const Handle<AssetObject>& assetObject)
{
    if (!assetObject)
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    {
        Mutex::Guard guard(m_mutex);

        auto it = m_assetObjects.Find(assetObject->GetName());

        if (it == m_assetObjects.End())
        {
            return HYP_MAKE_ERROR(Error, "AssetObject '{}' not found in package '{}'", assetObject->GetName(), m_name);
        }

        m_assetObjects.Erase(it);

        assetObject->m_package.Reset();
        assetObject->m_assetPath = {};
    }

    if (IsInitCalled())
    {
        OnAssetObjectRemoved(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectRemoved(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }

        /// TODO: remove the file
    }

    return {};
}

Handle<AssetObject> AssetPackage::GetAssetObject(WeakName assetName) const
{
    if (!assetName.IsValid())
    {
        return {};
    }

    Mutex::Guard guard(m_mutex);

    auto it = m_assetObjects.FindAs(assetName);

    if (it == m_assetObjects.End())
    {
        return {};
    }

    return *it;
}

Result AssetPackage::MergePackage(const Handle<AssetPackage>& package)
{
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

        // check if name is already taken in destination package
        if (currentAssetNames.Contains(desiredName))
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
            HYP_LOG(Assets, Warning, "Failed to remove asset '{}' from source package '{}' during merge: {}", asset->GetName(), package->GetName(), removeResult.GetError().GetMessage());

            continue;
        }

        if (Result addResult = AddAssetObject(asset); addResult.HasError())
        {
            HYP_LOG(Assets, Warning, "Failed to add asset '{}' to destination package '{}' during merge: {}", asset->GetName(), GetName(), addResult.GetError().GetMessage());
        }
    }

    Handle<AssetPackage> strongThis = MakeStrongRef(this);

    // needed for GetPackageFromPath() / GetSubpackage().
    // @TODO: Refactor to call these methods on AssetPackage directly?
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
    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    if (!parentPackage.IsValid())
    {
        return *m_name;
    }

    return parentPackage->BuildPackagePath() + "/" + *m_name;
}

AssetPath AssetPackage::BuildAssetPath(Name assetName) const
{
    if (!assetName.IsValid())
    {
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

    return assetPath;
}

void AssetPackage::Rename(Name name)
{
    if (m_name == name || !name.IsValid())
    {
        return;
    }

    // If the name starts with a '$', it's a transient package
    const char* str = name.LookupString();

    if (str[0] == '$')
    {
        m_flags |= APF_TRANSIENT | APF_HIDDEN;
    }

    name = SanitizeName(name);
    Name friendlyName = CreateFriendlyName(name);

    Mutex::Guard guard(m_mutex);

    // make sure we have a unique asset name within parent package
    if (Handle<AssetPackage> parentPackage = m_parentPackage.Lock(); parentPackage.IsValid())
    {
        Mutex::Guard guard2(parentPackage->m_mutex);
        name = GetUniqueName(name, parentPackage->m_subpackages);
    }

    m_name = name;
    m_friendlyName = friendlyName;
}

bool AssetPackage::HasAssetWithName(Name assetName) const
{
    if (!assetName.IsValid())
    {
        return false;
    }

    Mutex::Guard guard(m_mutex);
    return m_assetObjects.Contains(assetName);
}

Name AssetPackage::GetUniqueAssetName(Name baseName) const
{
    if (!baseName.IsValid())
    {
        return Name::Invalid();
    }

    Mutex::Guard guard(m_mutex);

    return GetUniqueAssetName_Internal(baseName);
}

Name AssetPackage::GetUniqueAssetName_Internal(Name baseName) const
{
    return GetUniqueName(baseName, m_assetObjects);
}

Name AssetPackage::GetUniqueSubpackageName_Internal(Name baseName) const
{
    return GetUniqueName(baseName, m_subpackages);
}

Result AssetPackage::Save(const FilePath& outputDirectory)
{
    HYP_SCOPE;
    AssertReady();

    if (IsTransient())
    {
        return HYP_MAKE_ERROR(Error, "Cannot save transient AssetPackage '{}'", m_name);
    }

    Handle<AssetRegistry> registry = m_registry.Lock();

    if (!registry)
    {
        return HYP_MAKE_ERROR(Error, "AssetPackage '{}' does not have a valid AssetRegistry", m_name);
    }

    Mutex::Guard guard(m_mutex);

    FilePath packageDir;

    // Build package save dir. `outputDirectory` /may/ just be a base path, where we'll append the package path to it
    const String packagePath = BuildPackagePath();

    Array<String> outputParts = outputDirectory.Split('/', '\\');
    Array<String> packageParts = packagePath.Split('/', '\\');

    SizeType packageStartIndex = 0;

    if (packageParts.Any() && outputParts.Any())
    {
        // Check if the last part of outputDirectory matches any part of packagePath
        const String& lastOutputPart = outputParts[outputParts.Size() - 1];

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
    }
    else if (!packageDir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' already exists and is not a directory", outputDirectory);
    }

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

    for (const Handle<AssetPackage>& subpackage : m_subpackages)
    {
        if (subpackage->IsTransient())
        {
            continue;
        }

        Result result = subpackage->Save(packageDir / *subpackage->GetName());

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save subpackage '{}' of package '{}': {}", subpackage->GetName(), m_name, result.GetError().GetMessage());
        }
    }

    if (!IsTransient() && m_packageDir.Length() != 0)
    {
        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            assetObject->m_filepath = packageDir / *assetObject->GetName();

            if (Result saveAssetResult = assetObject->Save(); saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
                continue;
            }

            assetObject->SetIsPersistentlyLoaded(false);
        }
    }

    return {};
}

Result AssetPackage::SaveManifest(ByteWriter& stream) const
{
    HYP_SCOPE;

    json::JSONObject manifestJson;
    ObjectToJSON(InstanceClass(), HypData(HandleFromThis()), manifestJson);

    // need to set virtual path property for loading
    manifestJson["Path"] = *BuildPackagePath();

    stream.WriteString(json::JSONValue(std::move(manifestJson)).ToString(true));

    return {};
}

void AssetPackage::AddDependency(const AssetPath& dependency)
{
    if (!dependency.chain || dependency.chain[0] == Name::Invalid() || dependency == AssetPath(BuildPackagePath()))
    {
        return;
    }

    if (!m_dependencies.Contains(dependency))
    {
        m_dependencies.PushBack(dependency);

        HYP_LOG(Assets, Debug, "Added dependency to package '{}': {}", m_name, dependency.ToString());
    }
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
    m_dependencies.Clear();

    const AssetPath thisPackagePath = AssetPath(BuildPackagePath());

    for (const String& path : relativePaths)
    {
        const AssetPath dependency = AssetPath::FromRelativePath(thisPackagePath, path);

        if (dependency.chain && dependency.chain[0] != Name::Invalid() && dependency != thisPackagePath)
        {
            m_dependencies.PushBack(dependency);
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
    : m_rootPath(rootPath)
{
}

AssetRegistry::~AssetRegistry()
{
}

void AssetRegistry::Init()
{
    HYP_SCOPE;

    HypObjectBase::Init();

    SetReady(true);

    Handle<AssetPackage> enginePackage = GetPackageFromPath("Engine", true);

    if (Result savePackageResult = enginePackage->Save(g_assetManager->GetBasePath()); savePackageResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to save 'Engine' package! Error was: {}", savePackageResult.GetError().GetMessage());
    }

    Handle<AssetPackage> memoryPackage = GetPackageFromPath("$Memory", true);

    LoadPackagesAsync(/* loadSubpackages */ false);

#ifdef HYP_EDITOR
    // Add transient package for imported assets in editor mode
    Handle<AssetPackage> importsPackage = GetPackageFromPath("$Import", true);
#endif
}

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
                    Handle<AssetPackage> package;

                    // build virtual package path from filesystem path
                    if (Result result = LoadPackageFromManifest(
                            manifestPath,
                            package,
                            loadSubpackages);
                        result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load package from manifest '{}': {}", manifestPath, result.GetError().GetMessage());

                        return;
                    }

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

    Mutex::Guard guard(m_mutex);

    m_rootPath = rootPath;
}

void AssetRegistry::SetPackages(const AssetPackageSet& packages)
{
    HYP_SCOPE;

    Proc<void(Handle<AssetPackage>)> initializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    initializePackage = [this, &initializePackage](const Handle<AssetPackage>& package)
    {
        Assert(package.IsValid());

        package->m_registry = WeakHandleFromThis();

        if (IsInitCalled())
        {
            InitObject(package);

            OnPackageAdded(package);
        }

        for (const Handle<AssetPackage>& subpackage : package->m_subpackages)
        {
            subpackage->m_parentPackage = package;
            subpackage->m_flags |= package->m_flags;

            initializePackage(subpackage);
        }
    };

    {
        Mutex::Guard guard(m_mutex);

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

Result AssetRegistry::AddPackage(Handle<AssetPackage>& package, bool mergeIfExists)
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

                // check if name is already taken in destination package
                if (destAssetNames.Contains(desiredName))
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
        package = existing;

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

    Proc<void(Handle<AssetPackage>)> initializePackage;
    initializePackage = [this, &initializePackage, &addedPackages](const Handle<AssetPackage>& pkg)
    {
        Assert(pkg != nullptr);

        pkg->m_registry = WeakHandleFromThis();

        if (IsInitCalled())
        {
            addedPackages.PushBack(pkg);
        }

        for (const Handle<AssetPackage>& sub : pkg->m_subpackages)
        {
            sub->m_parentPackage = pkg;
            sub->m_flags |= pkg->m_flags;

            initializePackage(sub);
        }
    };

    initializePackage(package);

    if (newParentPackage != nullptr)
    {
        Mutex::Guard guard(newParentPackage->m_mutex);

        package->m_parentPackage = newParentPackage;
        package->m_flags |= newParentPackage->m_flags;

        if (newParentPackage->IsInitCalled())
        {
            newParentPackage->OnSubpackageAdded(package);
        }

        newParentPackage->m_subpackages.Insert(package);
    }
    else // top-level package
    {
        Mutex::Guard guard(m_mutex);

        m_packages.Insert(package);
    }

    if (IsInitCalled())
    {
        for (const Handle<AssetPackage>& pkg : addedPackages)
        {
            InitObject(pkg);

            OnPackageAdded(pkg);
        }
    }

    return {};
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
    AssertReady();

    Handle<AssetPackage> subpackage;
    bool isNew = false;

    if (!parentPackage)
    {
        {
            Mutex::Guard guard(m_mutex);

            auto packageIt = m_packages.Find(subpackageName);

            if (createIfNotExist && packageIt == m_packages.End())
            {
                subpackage = CreateObject<AssetPackage>(subpackageName);
                subpackage->m_registry = WeakHandleFromThis();

                m_packages.Insert(subpackage);

                isNew = true;
            }
            else if (packageIt != m_packages.End())
            {
                subpackage = *packageIt;
            }
        }

        if (isNew && subpackage && IsInitCalled())
        {
            InitObject(subpackage);

            OnPackageAdded(subpackage);
        }

        return subpackage;
    }

    Optional<FilePath> saveOutputDir; // unset if no save needed

    {
        Mutex::Guard guard(parentPackage->m_mutex);

        auto packageIt = parentPackage->m_subpackages.Find(subpackageName);

        if (createIfNotExist && packageIt == parentPackage->m_subpackages.End())
        {
            subpackage = CreateObject<AssetPackage>(subpackageName);
            subpackage->m_registry = WeakHandleFromThis();
            subpackage->m_parentPackage = parentPackage;
            subpackage->m_flags |= parentPackage->m_flags;

            if (parentPackage->IsInitCalled())
            {
                // If parent package exists on disk, save this package:
                if (!parentPackage->IsTransient() && parentPackage->m_packageDir.Length() != 0)
                {
                    saveOutputDir = parentPackage->m_packageDir;
                }

                parentPackage->OnSubpackageAdded(subpackage);
            }

            parentPackage->m_subpackages.Insert(subpackage);
            isNew = true;
        }
        else if (packageIt != m_packages.End())
        {
            subpackage = *packageIt;
        }
    }

    if (isNew && subpackage && IsInitCalled())
    {
        InitObject(subpackage);

        OnPackageAdded(subpackage);

        if (saveOutputDir.HasValue())
        {
            if (Result saveResult = subpackage->Save(*saveOutputDir); saveResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save new subpackage '{}': {}", subpackage->GetName(), saveResult.GetError().GetMessage());
            }
        }
    }

    return subpackage;
}

void AssetRegistry::LoadSubpackages(const Handle<AssetPackage>& package, bool recursive)
{
    HYP_SCOPE;
    AssertReady();

    if (!package)
    {
        return;
    }

    if (package->m_registry.GetUnsafe() != this)
    {
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

        Handle<AssetPackage> subpackage;

        if (Result result = LoadPackageFromManifest(
                manifestPath,
                subpackage,
                /* loadSubpackages */ recursive);
            result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", manifestPath, result.GetError().GetMessage());

            continue;
        }

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
            HYP_LOG(Assets, Warning, "Subpackage with name '{}' already exists in package '{}', skipping loaded subpackage from '{}'", subpackage->GetName(), package->GetName(), manifestPath);
        }
    }
}

bool AssetRegistry::RemovePackage(AssetPackage* package)
{
    HYP_SCOPE;

    if (!package)
    {
        return false;
    }

    if (package->m_registry.GetUnsafe() != this)
    {
        return false;
    }

    package->m_registry.Reset();

    Handle<AssetPackage> strongPackage = MakeStrongRef(package);

    bool removed = false;

    {
        Mutex::Guard guard(m_mutex);

        if (package->m_parentPackage.IsValid())
        {
            Handle<AssetPackage> parentPackage = package->m_parentPackage.Lock();

            if (parentPackage.IsValid())
            {
                auto it = parentPackage->m_subpackages.Find(package->GetName());
                Assert(it != parentPackage->m_subpackages.End());

                if (parentPackage->IsInitCalled())
                {
                    parentPackage->OnSubpackageRemoved(strongPackage);
                }

                parentPackage->m_subpackages.Erase(it);

                removed = true;
            }
        }
        else
        {
            auto it = m_packages.Find(package->GetName());
            Assert(it != m_packages.End());

            m_packages.Erase(it);

            removed = true;
        }
    }

    if (removed)
    {
        OnPackageRemoved(strongPackage);

        return true;
    }

    return false;
}

Result AssetRegistry::LoadPackageFromManifest(
    const FilePath& manifestPath,
    Handle<AssetPackage>& outPackage,
    bool loadSubpackages)
{
    HYP_SCOPE;

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

    json::ParseResult parseResult = json::JSON::Parse(manifestStream);

    manifestStream.Close();

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    if (!parseResult.value.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must be an object");
    }

    const String packagePath = parseResult.value.Get("Path").ToString();
    outPackage = GetPackageFromPath(packagePath, true);

    HypData targetHypData = HypData(outPackage.ToRef());

    if (!JSONToObject(parseResult.value.AsObject(), outPackage->InstanceClass(), targetHypData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load package data from manifest");
    }

    outPackage->m_packageDir = dir;

    // Load dependency packages first (always, regardless of loadSubpackages flag)
    // Dependencies must be loaded before assets to ensure all referenced packages exist
    for (const AssetPath& dependencyPath : outPackage->GetDependencies())
    {
        if (!dependencyPath.IsValid())
        {
            HYP_LOG(Assets, Warning, "Invalid dependency path in package '{}'", outPackage->GetName());
            continue;
        }

        const String depPathStr = dependencyPath.ToString();

        HYP_LOG(Assets, Debug, "Loading dependency package '{}' for package '{}'", depPathStr, outPackage->GetName());

        Handle<AssetPackage> dependencyPackage = GetPackageFromPath(depPathStr, /* createIfNotExist */ false);

        if (!dependencyPackage.IsValid())
        {
            // Dependency package doesn't exist yet, try to load it from filesystem
            const FilePath basePath = g_assetManager->GetBasePath();
            const FilePath depFullPath = basePath / depPathStr;
            const FilePath depManifestPath = depFullPath / "PackageManifest.json";

            if (depManifestPath.Exists() && !depManifestPath.IsDirectory())
            {
                if (Result result = LoadPackageFromManifest(
                        depManifestPath,
                        dependencyPackage,
                        /* loadSubpackages */ false);
                    result.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to load dependency package '{}' from manifest '{}': {}", depPathStr, depManifestPath, result.GetError().GetMessage());
                    continue;
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Dependency package '{}' for package '{}' not found at '{}'", depPathStr, outPackage->GetName(), depManifestPath);
                continue;
            }
        }
    }

    // load assets
    for (const FilePath& entry : dir.GetAllFilesInDirectory())
    {
        // only iterate over files (manifests)
        // TODO: Add param to `GetAllFilesInDirectory` to filter by extension instead of checking each entry
        if (entry.GetExtension() != "json")
        {
            continue;
        }

        if (entry.Basename() == "PackageManifest.json")
        {
            // Skip the package manifest itself
            continue;
        }

        const FilePath dataPath = entry.StripExtension();

        if (!dataPath.Exists() || dataPath.IsDirectory())
        {
            HYP_LOG(Assets, Warning, "Asset data file '{}' for asset manifest '{}' does not exist or is not a file!", dataPath, entry);

            continue;
        }

        FileBufferedReaderSource manifestSource { entry };
        BufferedReader manifestStream { &manifestSource };

        FileBufferedReaderSource dataSource { dataPath };
        BufferedReader dataStream { &dataSource };

        Handle<AssetObject> assetObject;

        if (Result loadAssetResult = AssetObject::Load(manifestStream, dataStream, assetObject); loadAssetResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to load asset from manifest '{}': {}", entry, loadAssetResult.GetError().GetMessage());

            continue;
        }

        AssertDebug(assetObject != nullptr);

        // set filepath so it doesn't get double saved on adding to package
        assetObject->m_filepath = dataPath;

        if (Result addAssetResult = outPackage->AddAssetObject(assetObject); addAssetResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to add asset object '{}' to package '{}': {}", assetObject->GetName(), outPackage->GetName(), addAssetResult.GetError().GetMessage());

            continue;
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
                    Handle<AssetPackage> subpackage;

                    // Load WITH sub-subpackages recursively
                    if (Result result = LoadPackageFromManifest(
                            entry,
                            subpackage,
                            /* loadSubpackages */ true);
                        result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", entry, result.GetError().GetMessage());
                        break;
                    }

                    if (subpackage.IsValid())
                    {
                        subpackage->m_parentPackage = outPackage;
                        subpackage->m_flags |= outPackage->m_flags;

                        if (outPackage->IsInitCalled())
                        {
                            outPackage->OnSubpackageAdded(subpackage);
                        }

                        outPackage->m_subpackages.Insert(subpackage);
                    }

                    break;
                }
            }
        }
    }

    return {};
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::u32char('/') || *it == utf::u32char('\\'))
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

    String pathString = path;
    Array<String> pathStringSplit = pathString.Split('/', '\\');

    String assetName;

    pathString = String::Join(pathStringSplit, '/');

    AssetRegistryPathType pathType = AssetRegistryPathType::PACKAGE;

    Handle<AssetPackage> assetPackage;

    {
        assetPackage = GetPackageFromPath_Internal(pathString, pathType, /* createIfNotExist */ true, assetName);

        if (pathType == AssetRegistryPathType::ASSET)
        {
            const Name baseName = assetName.Any() ? CreateNameFromDynamicString(assetName) : NAME("Unnamed");

            assetObject->m_name = assetPackage->GetUniqueAssetName(baseName);
        }
    }

    return assetPackage->AddAssetObject(assetObject);
}

HYP_DISABLE_OPTIMIZATION;
void AssetRegistry::RegisterAssetsRecursively(
    const UTF8StringView& packagePath,
    const HypData& target,
    bool forceRelocation,
    ProcRef<String(const AssetObject&)> getObjectSubpath)
{
    HYP_SCOPE;

    if (!target.IsValid() || target.IsNull())
    {
        return;
    }

    /// @TODO: Change to a Stack, recursion could get impressively deep.

    HashSet<const HypObjectBase*> visited; // to avoid infinite recursion

    Proc<void(const Handle<AssetPackage>&, const HypData&, HashSet<AssetPath>&)> iterate;
    iterate = [&](const Handle<AssetPackage>& inPackage, const HypData& current, HashSet<AssetPath>& outDeps) -> void
    {
        Assert(inPackage != nullptr);

        if (!current.IsValid() || current.IsNull())
        {
            return;
        }

        HashSet<AssetPath> currDeps;
        HYP_DEFER({
            if (currDeps.Any())
            {
                outDeps.Merge(std::move(currDeps));
            }
        });

        {
            HypObjectBase* pObject = current.TryGet<HypObjectBase*>().GetOr(nullptr);
            if (pObject && !visited.Insert(pObject).second)
            {
                HYP_LOG_TEMP("Already visited {} with ID {}, skipping to avoid infinite recursion",
                    pObject->InstanceClass() ? *pObject->InstanceClass()->GetName() : "<no class>", pObject->Id());

                return;
            }
        }

        Handle<AssetPackage> parentPackage = inPackage;
        Handle<AssetObject> assetObject;

        if (current.Is<AssetObject>())
        {
            assetObject = MakeStrongRef(&current.Get<AssetObject>());
            Assert(assetObject != nullptr);

            const String packagePathWithSubpath = getObjectSubpath
                ? packagePath + "/" + getObjectSubpath(*assetObject)
                : String(packagePath);

            Handle<AssetPackage> newPackage = GetPackageFromPath(packagePathWithSubpath, /* createIfNotExist */ true);
            Assert(newPackage != nullptr);

            if (!newPackage->IsSubpackageOf(*inPackage))
            {
                inPackage->AddDependency(AssetPath(packagePathWithSubpath));
            }

            parentPackage = std::move(newPackage);
        }

        if (current.Is<HypDataArray>()) // array needs special handling: iterate over elements (if possible)
        {
            HypDataArray& array = current.Get<HypDataArray>();

            if (!array.CanGetElementByIndex())
            {
                HYP_LOG(Assets, Error, "Cannot iterate over {}: not indexable", LookupTypeName(current.GetTypeId()));
                return;
            }

            SizeType size = array.Size();

            for (SizeType i = 0; i < size; ++i)
            {
                HYP_LOG_TEMP("Iterating element {} of array of type {}", i, LookupTypeName(current.GetTypeId()));

                HypData element;
                if (!array.ElementAt(i, element))
                {
                    HYP_LOG(Assets, Warning, "Failed to get element at index {} of array of type {}", i, LookupTypeName(current.GetTypeId()));
                    continue;
                }

                iterate(parentPackage, element, currDeps);
            }

            return;
        }

        // special handling for Entity: needs to collect from components
        // @TODO: Move to a method that can be overridden for custom handling?
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
                        const auto componentRef = entityManager->TryGetComponent(typeId, &entity);
                        if (!componentRef.HasValue())
                        {
                            continue;
                        }

                        HYP_LOG_TEMP("Iterating component of type {} for Entity {}", LookupTypeName(typeId), entity.Id());

                        iterate(parentPackage, HypData(componentRef), currDeps);
                    }
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Entity {} has no valid EntityManager, cannot iterate components", entity.Id());
            }
        }

        const TypeId typeId = current.GetTypeId();
        const HypClass* hypClass = GetClass(typeId);

        if (!hypClass) // no HypClass; not an object we can iterate over.
        {
            return;
        }

        for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD, /* deep */ true))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute("property").IsValid())
            {
                // skip non-property members if they have the 'property' attribute (synthetic property)
                continue;
            }

            HypData memberData;
            switch (member.GetMemberType())
            {
            case HypMemberType::TYPE_PROPERTY:
            {
                const HypProperty* property = static_cast<const HypProperty*>(&member);
                memberData = property->Get(current);
            }
            break;
            case HypMemberType::TYPE_FIELD:
            {
                const HypField* field = static_cast<const HypField*>(&member);
                memberData = field->Get(current);
            }
            break;
            default:
                HYP_UNREACHABLE();
                break;
            }

            if (!memberData.IsValid() || memberData.IsNull())
            {
                continue;
            }

            iterate(parentPackage, memberData, currDeps);
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

    HashSet<AssetPath> deps;

    iterate(rootPackage, target, deps);
}
HYP_ENABLE_OPTIMIZATION;

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

} // namespace hyperion
