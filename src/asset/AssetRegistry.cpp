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

void AssetDataResourceBase::Initialize()
{
    Mutex::Guard guard(m_mutex);

    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    BufferedReader stream;

    HYP_DEFER({
        stream.Close();

        if (stream.GetSource() != nullptr)
        {
            delete stream.GetSource();
        }
    });

    if (Result openStreamResult = assetObject->OpenReadStream(stream); openStreamResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to open stream for asset '{}': {}", assetObject->GetPath().ToString(), openStreamResult.GetError().GetMessage());

        return;
    }

    Assert(stream.GetSource() != nullptr);

    HypData value;

    FBOMLoadContext context;
    FBOMReader reader { FBOMReaderConfig {} };
    FBOMResult err;

    if ((err = reader.Deserialize(context, stream, value)))
    {
        HYP_LOG(Assets, Error, "Failed to load asset {}\n\tMessage: {}", assetObject->GetPath().ToString(), err.message);

        return;
    }

    Extract_Internal(std::move(value));
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

    FileByteWriter byteWriter { path };
    HYP_DEFER({ byteWriter.Close(); });

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
        AssetDataResourceBase* resourceCasted = static_cast<AssetDataResourceBase*>(m_resource);
        resourceCasted->m_assetObject = WeakHandleFromThis();

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

    if (Handle<AssetPackage> package = GetPackage())
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

    AssetDataResourceBase* resourceCasted = static_cast<AssetDataResourceBase*>(m_resource);

    Mutex::Guard guard(resourceCasted->m_mutex);

    const FilePath path = m_filepath;

    if (path.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset path is empty, cannot save");
    }

    const FilePath dir = path.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        HYP_BREAKPOINT;
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

    return resourceCasted->Save_Internal(path);
}

Result AssetObject::SaveManifest(ByteWriter& stream) const
{
    json::JSONObject manifestJson;
    ObjectToJSON(InstanceClass(), HypData(AnyRef(const_cast<AssetObject*>(this))), manifestJson);

    manifestJson["$Class"] = *InstanceClass()->GetName();

    stream.WriteString(json::JSONValue(std::move(manifestJson)).ToString(true));

    return {};
}

Result AssetObject::LoadAssetFromManifest(BufferedReader& stream, Handle<AssetObject>& outAssetObject)
{
    HYP_LOG(Assets, Debug, "Loading asset from manifest stream");

    if (!stream.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Stream is not open");
    }

    json::ParseResult parseResult = json::JSON::Parse(stream);

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

    HypData hypData;
    if (!hypClass->CreateInstance(hypData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create instance of class '{}'", classNameValue.AsString());
    }

    AssertDebug(hypData.Is<Handle<AssetObject>>());

    // remove class property
    jsonObject.Erase("$Class");

    if (!JSONToObject(jsonObject, hypClass, hypData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to deserialize asset object from manifest JSON");
    }

    outAssetObject = hypData.Get<Handle<AssetObject>>();

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

    bool isSaved = false;

    {
        Mutex::Guard guard(m_mutex);

        isSaved = !IsTransient() && m_packageDir.Length() != 0;

        assetObjects.Reserve(m_assetObjects.Size());
        subpackages.Reserve(m_subpackages.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            if (IsTransient())
            {
                // transient data isn't saved to disk so we have to keep it in memory
                assetObject->SetIsPersistentlyLoaded(true);
            }
            else if (isSaved)
            {
                assetObject->m_filepath = m_packageDir / *assetObject->GetName();
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
        if (isSaved)
        {
            // save the file in our package
            Result saveAssetResult = assetObject->Save();

            if (saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
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

    bool isSaved = false;

    {
        Mutex::Guard guard(m_mutex);

        isSaved = !IsTransient() && m_packageDir.Length() != 0;

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
            else if (isSaved)
            {
                assetObject->m_filepath = m_packageDir / *assetObject->GetName();
            }

            InitObject(assetObject);

            newAssetObjects.PushBack(assetObject);
        }
    }

    if (IsInitCalled())
    {
        for (const Handle<AssetObject>& assetObject : newAssetObjects)
        {
            if (isSaved)
            {
                // save the file in our package
                Result saveAssetResult = assetObject->Save();

                if (saveAssetResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
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

    if (assetObject->GetPackage() == this)
    {
        // already added, fine
        return {};
    }

    if (assetObject->GetPackage().IsValid())
    {
        HYP_LOG(Assets, Warning, "AssetObject '{}' already belongs to another package!", assetObject->GetName());
    }

    assetObject->m_package = WeakHandleFromThis();
    assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

    bool isSaved = false;

    {
        Mutex::Guard guard(m_mutex);

        isSaved = !IsTransient() && m_packageDir.Length() != 0;

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
        else if (isSaved)
        {
            assetObject->m_filepath = m_packageDir / *assetObject->GetName();
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

        if (isSaved)
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
    ObjectToJSON(InstanceClass(), HypData(AnyRef(const_cast<AssetPackage*>(this))), manifestJson);

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

            HYP_LOG(Assets, Debug, "Loading packages from root path: {}", rootPath);

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
                HYP_LOG(Assets, Debug, "Searching for package manifest in directory: {}", dir);

                bool packageFound = false;

                for (const FilePath& entry : dir.GetAllFilesInDirectory())
                {
                    if (entry.Basename() != "PackageManifest.json")
                    {
                        continue;
                    }

                    FileBufferedReaderSource source { entry };
                    BufferedReader manifestStream { &source };

                    if (!manifestStream.IsOpen())
                    {
                        HYP_LOG(Assets, Error, "Failed to open manifest file '{}'", entry);

                        continue;
                    }

                    Handle<AssetPackage> package;

                    const String packagePath = FilePath::Relative(dir, g_assetManager->GetBasePath());

                    // build virtual package path from filesystem path
                    if (Result result = LoadPackageFromManifest(dir, packagePath, manifestStream, package, /* loadSubpackages */ true); result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load package from manifest '{}': {}", packagePath, result.GetError().GetMessage());

                        continue;
                    }

                    if (!package.IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' is invalid!", entry);

                        continue;
                    }

                    if (!package->GetName().IsValid())
                    {
                        HYP_LOG(Assets, Error, "Package at path '{}' has an invalid name!", entry);

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

            HYP_LOG(Assets, Debug, "Loaded {} packages from root path '{}'", rootPackages.Size(), rootPath);
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

    Handle<AssetPackage> strongPackage = package->HandleFromThis();

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

Result AssetRegistry::LoadPackageFromManifest(const FilePath& dir, UTF8StringView packagePath, BufferedReader& manifestStream, Handle<AssetPackage>& outPackage, bool loadSubpackages)
{
    HYP_SCOPE;

    HYP_LOG(Assets, Debug, "Loading package from manifest: {}", packagePath);

    json::ParseResult parseResult = json::JSON::Parse(manifestStream);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    if (!parseResult.value.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must be an object");
    }

    outPackage = GetPackageFromPath(packagePath, true);

    HypData targetHypData = HypData(outPackage.ToRef());

    if (!JSONToObject(parseResult.value.AsObject(), outPackage->InstanceClass(), targetHypData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load package data from manifest");
    }

    const FilePath packageDir = dir / outPackage->BuildPackagePath();

    if (!packageDir.Exists() || !packageDir.IsDirectory())
    {
        HYP_LOG(Assets, Warning, "Package directory '{}' does not exist or is not a directory", packageDir);

        return {};
    }

    outPackage->m_packageDir = packageDir;

    // Load AssetObjects from manifest files
    for (const FilePath& entry : packageDir.GetAllFilesInDirectory())
    {
        if (entry.GetExtension() != "json")
        {
            continue;
        }

        if (entry.Basename() == "PackageManifest.json")
        {
            // Skip the package manifest itself
            continue;
        }

        FileBufferedReaderSource source { entry };
        BufferedReader assetManifestStream { &source };

        Handle<AssetObject> assetObject;

        if (Result loadAssetResult = AssetObject::LoadAssetFromManifest(assetManifestStream, assetObject); loadAssetResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to load asset from manifest '{}': {}", entry, loadAssetResult.GetError().GetMessage());

            continue;
        }

        if (Result addAssetResult = outPackage->AddAssetObject(assetObject); addAssetResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to add asset object '{}' to package '{}': {}", assetObject->GetName(), outPackage->GetName(), addAssetResult.GetError().GetMessage());

            continue;
        }
    }

    if (loadSubpackages)
    {
        // Load subpackages

        for (const FilePath& subdirectory : packageDir.GetSubdirectories())
        {
            for (const FilePath& entry : subdirectory.GetAllFilesInDirectory())
            {
                if (entry.Basename() == "PackageManifest.json")
                {
                    FileBufferedReaderSource source { entry };
                    BufferedReader subpackageManifestStream { &source };

                    Handle<AssetPackage> subpackage;

                    // build virtual package path from filesystem path
                    const String packagePath = FilePath::Relative(subdirectory, g_assetManager->GetBasePath());

                    if (Result result = LoadPackageFromManifest(subdirectory, packagePath, subpackageManifestStream, subpackage, /* loadSubpackages */ true); result.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to load subpackage from manifest '{}': {}", packagePath, result.GetError().GetMessage());

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

#pragma endregion AssetRegistry

} // namespace hyperion
