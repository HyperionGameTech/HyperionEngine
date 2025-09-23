/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypDataJSONHelpers.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/json/JSON.hpp>

#include <system/MessageBox.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

//! for debugging
static constexpr bool g_disableAssetUnload = false;

HYP_API WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage)
{
    if (!assetPackage.IsValid())
    {
        return {};
    }

    return assetPackage->GetName();
}

HYP_API WeakName AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return {};
    }

    return assetObject->GetName();
}

#pragma region AssetResourceBase

Result AssetDataResourceBase::LoadFromStream(BufferedReader& stream)
{
    FBOMLoadContext context;
    FBOMReader reader { FBOMReaderConfig {} };
    FBOMResult err;

    HypData data;
    if ((err = reader.Deserialize(context, stream, data)))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load asset: {}", err.message);
    }

    Extract_Internal(std::move(data));

    return {};
}

void AssetDataResourceBase::Initialize()
{
    Mutex::Guard guard(m_mutex);

    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    BufferedReader stream;

    HYP_DEFER({
        if (stream.GetSource() != nullptr)
        {
            delete stream.GetSource();
        }

        stream.Close();
    });

    if (Result openStreamResult = assetObject->OpenReadStream(stream); openStreamResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to open stream for asset '{}': {}", assetObject->GetPath().ToString(), openStreamResult.GetError().GetMessage());

        return;
    }

    if (Result loadResult = LoadFromStream(stream); loadResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", assetObject->GetPath().ToString(), loadResult.GetError().GetMessage());

        return;
    }
}

void AssetDataResourceBase::Destroy()
{
    HYP_LOG(Assets, Debug, "Unloading asset '{}'", m_assetObject.GetUnsafe()->IsRegistered() ? *m_assetObject.GetUnsafe()->GetPath().ToString() : *m_assetObject.GetUnsafe()->GetName());

    Unload_Internal();
}

Result AssetDataResourceBase::Save_Internal(const FilePath& path)
{
    // mutex will already be locked by the asset object that owns this resource

    AssetObject* assetObject = m_assetObject.GetUnsafe();
    Assert(assetObject != nullptr);

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetAssetTypeId());
    Assert(marshal != nullptr);

    FBOMObject object;

    AnyRef assetRef = GetAssetRef();

    if (!assetRef)
    {
        return HYP_MAKE_ERROR(Error, "Asset data reference is invalid!");
    }

    if (FBOMResult err = marshal->Serialize(assetRef, object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to serialize asset: {}", err.message);
    }

    Assert(object.GetType().GetNativeTypeId() == GetAssetTypeId(),
        "Object must have a native TypeId associated to be deserialized properly! Expected TypeId {}, Got serialized type: {}",
        GetAssetTypeId().Value(),
        object.GetType().ToString(true));

    writer.Append(std::move(object));

    FileByteWriter byteWriter { path };
    if (FBOMResult err = writer.Emit(&byteWriter))
    {
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk");
    }

    HYP_LOG(Assets, Debug, "Saved asset to '{}'", path);

    return {};
}

#pragma endregion AssetResourceBase

#pragma region AssetObject

AssetObject::AssetObject()
    : m_resource(&GetNullResource()),
      m_flags(AOF_NONE),
      m_pool(nullptr)
{
}

AssetObject::AssetObject(Name name)
    : m_name(name),
      m_resource(&GetNullResource()),
      m_flags(AOF_NONE),
      m_pool(nullptr)
{
}

AssetObject::~AssetObject()
{
    // need to release before freeing resource or we'll deadlock
    m_persistentResource.Reset();

    if (m_resource != nullptr && !m_resource->IsNull())
    {
        Assert(m_pool != nullptr);

        m_pool->Free(m_resource);
    }
}

void AssetObject::Init()
{
    if (m_resource && !m_resource->IsNull())
    {
        AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);
        resource->m_assetObject = WeakHandleFromThis();

        if ((m_flags[AOF_PERSISTENT] || g_disableAssetUnload) && !m_persistentResource)
        {
            m_persistentResource = ResourceHandle(*m_resource);
        }
    }

    SetReady(true);
}

void AssetObject::SetIsPersistentlyLoaded(bool persistentlyLoaded)
{
    m_flags[AOF_PERSISTENT] = persistentlyLoaded;

    if (persistentlyLoaded)
    {
        if (!m_persistentResource && m_resource && !m_resource->IsNull())
        {
            // shouldInitialize is false since it should already be in memory and we use this for transient assets,
            // meaning we can't load the data from disk
            m_persistentResource = ResourceHandle(*m_resource, /* shouldInitialize */ false);
            Assert(m_persistentResource);
        }

        return;
    }

    if (g_disableAssetUnload)
    {
        return;
    }

    m_persistentResource.Reset();
}

Result AssetObject::Rename(Name name)
{
    if (name == m_name)
    {
        // same name, do nothing
        return {};
    }

    Handle<AssetPackage> package = GetPackage();
    if (package.IsValid())
    {
        Handle<AssetObject> strongThis = HandleFromThis();

        if (Result result = package->RemoveAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset object '{}' from package '{}': {}", m_name, package->GetName(), result.GetError().GetMessage());

            return result;
        }

        m_name = name;

        if (Result result = package->AddAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to rename asset object '{}' to '{}': {}", m_name, name, result.GetError().GetMessage());

            return result;
        }
    }
    else
    {
        m_name = name;
    }

    return {};
}

bool AssetObject::IsLoaded() const
{
    if (!m_resource || m_resource->IsNull())
    {
        return false;
    }

    return static_cast<AssetDataResourceBase*>(m_resource)->IsInitialized();
}

Result AssetObject::Save() const
{
    if (!m_resource || m_resource->IsNull())
    {
        return HYP_MAKE_ERROR(Error, "No resource set, cannot save");
    }

    AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);

    Mutex::Guard guard(resource->m_mutex);

    const FilePath path = m_filepath;

    if (path.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset path is empty, cannot save");
    }

    const FilePath dir = path.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid directory, cannot save asset", dir);
    }

    FileByteWriter manifestWriter { path.StripExtension() + ".json" };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}'", m_name);
    }

    if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
    }

    manifestWriter.Close();

    return resource->Save_Internal(path);
}

Result AssetObject::SaveManifest(ByteWriter& stream) const
{
    json::JSONObject manifestJson;
    ObjectToJSON(InstanceClass(), HypData(HandleFromThis()), manifestJson);

    manifestJson["$Class"] = *InstanceClass()->GetName();

    stream.WriteString(json::JSONValue(std::move(manifestJson)).ToString(true));

    return {};
}

Result AssetObject::Load(
    BufferedReader& manifestStream,
    BufferedReader& dataStream,
    Handle<AssetObject>& outAssetObject)
{
    if (!manifestStream.IsOpen() || !dataStream.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Manifest or data stream not open");
    }

    json::ParseResult parseResult = json::JSON::Parse(manifestStream);

    manifestStream.Close(); // not needed anymore

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    if (!parseResult.value.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must be an object");
    }

    json::JSONObject jsonObject = std::move(parseResult.value.AsObject());
    json::JSONValue classNameValue = jsonObject["$Class"];

    if (!classNameValue.IsString())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must contain a 'class' string");
    }

    const HypClass* hypClass = GetClass(classNameValue.AsString());

    if (!hypClass)
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' not found!", classNameValue.AsString());
    }

    if (!hypClass->IsDerivedFrom(AssetObject::Class()))
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' is not derived from AssetObject!", classNameValue.AsString());
    }

    HypData targetData;
    if (!hypClass->CreateInstance(targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create instance of class '{}'", classNameValue.AsString());
    }

    // remove class property
    jsonObject.Erase("$Class");

    if (!JSONToObject(jsonObject, hypClass, targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to deserialize asset object from manifest JSON");
    }

    // Load the asset's data
    const Handle<AssetObject>& assetObject = targetData.Get<Handle<AssetObject>>();
    Assert(assetObject != nullptr);

    AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(assetObject->m_resource);
    Assert(resource != nullptr);

    if (Result loadResult = resource->LoadFromStream(dataStream); loadResult.HasError())
    {
        return loadResult;
    }

    outAssetObject = assetObject;

#if 0
    FBOMLoadContext loadContext {};
    FBOMObject dataObject;

    FBOMReader dataReader(FBOMReaderConfig {});
    if (FBOMResult result = dataReader.ReadObject(loadContext, &dataStream, dataObject, nullptr, /* deserializeObject */ true); !result.IsOK())
    {
        return HYP_MAKE_ERROR(Error, "Failed to read asset data: {}", result.message);
    }

    if (!dataObject.m_deserializedObject || !dataObject.m_deserializedObject->Is<Handle<AssetObject>>())
    {
        return HYP_MAKE_ERROR(Error, "Deserialized asset data is not a valid AssetObject");
    }

    outAssetObject = std::move(dataObject.m_deserializedObject->Get<Handle<AssetObject>>());
#endif

    return {};
}

Result AssetObject::OpenReadStream(BufferedReader& stream) const
{
    Handle<AssetPackage> package = GetPackage();
    if (!package.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Package is invalid");
    }

    return package->OpenAssetReadStream(m_name, stream);
}

#pragma endregion AssetObject

#pragma region AssetPackage

AssetPackage::AssetPackage()
    : AssetPackage(Name::Invalid())
{
}

AssetPackage::AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags)
    : m_name(name),
      m_flags(flags)
{
    if (name.IsValid())
    {
        const char* str = name.LookupString();
        if (str[0] == '$')
        {
            m_flags |= APF_TRANSIENT | APF_HIDDEN;
        }

        String friendlyNameStr;

        for (auto it : UTF8StringView(str))
        {
            if (utf::utf32Isalpha(it) || utf::utf32Isdigit(it))
            {
                friendlyNameStr.Append(it);
            }
        }

        m_friendlyName = CreateNameFromDynamicString(StringUtil::ToPascalCase(friendlyNameStr, true));
    }
}

void AssetPackage::Init()
{
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
                const FilePath newAssetPath = m_packageDir / *assetObject->GetName();

                if (assetObject->m_filepath != newAssetPath)
                {
                    assetObject->m_filepath = newAssetPath;

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
                const FilePath newAssetPath = m_packageDir / *assetObject->GetName();

                if (assetObject->m_filepath != newAssetPath)
                {
                    assetObject->m_filepath = newAssetPath;

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
            const FilePath newAssetPath = m_packageDir / *assetObject->GetName();

            if (newAssetPath != assetObject->m_filepath)
            {
                assetObject->m_filepath = newAssetPath;

                doSaveAsset = true; // asset path changed, we need to save
            }
        }

        auto assetObjectsIt = m_assetObjects.Find(assetObject->GetName());

        if (assetObjectsIt != m_assetObjects.End())
        {
            if (*assetObjectsIt != assetObject)
            {
                return HYP_MAKE_ERROR(Error, "AssetObject with name '{}' already exists in package '{}'", assetObject->GetName(), m_name);
            }

            // already exists, fine
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
        return *GetFriendlyName();
    }

    return parentPackage->BuildPackagePath() + "/" + *GetFriendlyName();
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
        chain.PushBack(parentPackage->GetFriendlyName());

        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    chain.Reverse();
    chain.PushBack(m_name);
    chain.PushBack(assetName);

    AssetPath assetPath;
    assetPath.SetChain(chain);

    return assetPath;
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
    int counter = 0;
    String str = *baseName;

    while (m_assetObjects.Contains(str))
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

    FilePath packageDirectory = outputDirectory / BuildPackagePath();

    if (!packageDirectory.Exists())
    {
        if (!packageDirectory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create package directory '{}'", packageDirectory);
        }
    }
    else if (!packageDirectory.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' already exists and is not a directory", packageDirectory);
    }

    const FilePath manifestPath = packageDirectory / "PackageManifest.json";

    FileByteWriter manifestWriter { manifestPath };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for package '{}'", m_name);
    }

    if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save manifest for package '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
    }

    manifestWriter.Close();

    m_packageDir = packageDirectory;

    for (const Handle<AssetPackage>& subpackage : m_subpackages)
    {
        if (subpackage->IsTransient())
        {
            continue;
        }

        Result result = subpackage->Save(outputDirectory);

        if (result.HasError())
        {
            return result.GetError();
        }
    }

    if (!IsTransient() && m_packageDir.Length() != 0)
    {
        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            assetObject->m_filepath = m_packageDir / *assetObject->GetName();

            Result result = assetObject->Save();

            if (result.HasError())
            {
                return result.GetError();
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

    stream.WriteString(json::JSONValue(std::move(manifestJson)).ToString(true));

    return {};
}

Result AssetPackage::OpenAssetReadStream(Name assetName, BufferedReader& stream) const
{
    HYP_SCOPE;
    AssertReady();

    if (!assetName.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Asset name is invalid");
    }

    Mutex::Guard guard(m_mutex);
    auto it = m_assetObjects.Find(assetName);

    if (it == m_assetObjects.End())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject '{}' not found in package '{}'", assetName, m_name);
    }

    Handle<AssetObject> assetObject = *it;

    if (!m_packageDir.IsDirectory())
    {
        // package not saved

        return HYP_MAKE_ERROR(Error, "Package not saved; cannot load asset");
    }

    FileBufferedReaderSource* source = new FileBufferedReaderSource(m_packageDir / *assetObject->GetName());

    stream = BufferedReader { source };

    if (!stream.IsOpen())
    {
        delete source;

        return HYP_MAKE_ERROR(Error, "Failed to open stream for asset '{}'", assetName);
    }

    return {};
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

    SetReady(true);

    Handle<AssetPackage> memoryPackage = GetPackageFromPath("$Memory", true);
    Handle<AssetPackage> enginePackage = GetPackageFromPath("$Engine", true);

    LoadPackagesAsync();

#ifdef HYP_EDITOR
    // Add transient package for imported assets in editor mode
    Handle<AssetPackage> importsPackage = GetPackageFromPath("$Import", true);
#endif
}

void AssetRegistry::LoadPackagesAsync()
{
    HYP_SCOPE;

    FilePath rootPath = g_assetManager->GetBasePath();

    if (!rootPath.Exists() || !rootPath.IsDirectory())
    {
        // nothing to load if it doesnt exist
        return;
    }

    TaskSystem::GetInstance().Enqueue([this, weakThis = WeakHandleFromThis(), rootPath]()
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

                for (const FilePath& entry : dir.GetAllFilesInDirectory())
                {
                    if (entry.GetExtension() != "packagemanifest")
                    {
                        continue;
                    }

                    Handle<AssetPackage> package;

                    const FilePath packageDir = dir / entry.StripExtension();

                    // build virtual package path from filesystem path
                    if (Result result = LoadPackageFromManifest(
                            packageDir,
                            FilePath::Relative(dir, rootPath),
                            package,
                            /* loadSubpackages */ true);
                        result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load package from manifest '{}': {}", packageDir, result.GetError().GetMessage());

                        continue;
                    }

                    if (!package.IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' is invalid!", packageDir);

                        continue;
                    }

                    if (!package->GetName().IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' has an invalid name!", packageDir);

                        continue;
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

            iterateDirectory(rootPath);
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

    const String fullPath = package->BuildPackagePath();
    Handle<AssetPackage> existing = GetPackageFromPath(fullPath, /* createIfNotExist */ false);

    if (existing.IsValid())
    {
        if (existing == package)
        {
            // already added, return early
            return {};
        }

        if (!mergeIfExists)
        {
            return HYP_MAKE_ERROR(Error, "Package with path '{}' already exists", fullPath);
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

Handle<AssetPackage> AssetRegistry::GetSubpackage(const Handle<AssetPackage>& parentPackage, Name subpackageName, bool createIfNotExist)
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
    }

    return subpackage;
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
    const String& basePackagePath,
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

    const String packageName = parseResult.value.Get("name").ToString();

    if (packageName.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must contain a non-empty 'name' field");
    }

    Array<String> parts;
    if (!basePackagePath.Empty())
    {
        parts = basePackagePath.Split('/', '\\');
    }

    parts.PushBack(packageName);

    const String packagePath = String::Join(parts, '/');

    outPackage = GetPackageFromPath(packagePath, true);

    HypData targetHypData = HypData(outPackage.ToRef());

    if (!JSONToObject(parseResult.value.AsObject(), outPackage->InstanceClass(), targetHypData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load package data from manifest");
    }

    outPackage->m_packageDir = dir;

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

    if (loadSubpackages)
    {
        // Load subpackages

        for (const FilePath& subdirectory : dir.GetSubdirectories())
        {
            for (const FilePath& entry : subdirectory.GetAllFilesInDirectory())
            {
                if (entry.Basename() == "PackageManifest.json")
                {
                    Handle<AssetPackage> subpackage;

                    if (Result result = LoadPackageFromManifest(
                            entry,
                            packagePath,
                            subpackage,
                            /* loadSubpackages */ true);
                        result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", entry, result.GetError().GetMessage());

                        continue;
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

        return currentPackage;
    case AssetRegistryPathType::ASSET:
        outAssetName = std::move(currentString);

        return currentPackage;
    default:
        HYP_UNREACHABLE();
    }
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
