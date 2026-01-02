/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/reflection/HypDataJSONHelpers.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/io/BufferedByteReader.hpp>
#include <core/io/ByteWriter.hpp>

#include <core/json/JSON.hpp>

#include <system/MessageBox.hpp>

#include <engine/EngineDriver.hpp>

#include <AssetObject.generated.inl>

namespace Hyperion {

//! for debugging
static constexpr bool DebugDisableUnload = false;

HYP_API extern const FilePath& GetResourceDirectory();

extern HYP_NODISCARD String SanitizeName(const UTF8StringView& nameStr);
extern HYP_NODISCARD Name SanitizeName(Name name);
extern HYP_NODISCARD Name CreateFriendlyName(Name name);

template <class T>
static Name GetUniqueName(Name baseName, T&& elements)
{
    HYP_SCOPE;

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

#pragma region AssetResourceBase

Result AssetDataResourceBase::LoadFromStream(BufferedReader& stream)
{
    HYP_SCOPE;

    FBOMLoadContext context;
    FBOMReader reader { FBOMReaderConfig {} };
    FBOMResult err;

    BoxedValue boxed;
    if ((err = reader.Deserialize(context, stream, boxed)))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load asset: {}", err.message);
    }

    Extract_Internal(boxed.ToRef());
    boxed.Reset();

    return {};
}

void AssetDataResourceBase::Initialize()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    //    if (IsDataLoaded())
    //    {
    //        HYP_LOG(Assets, Debug, "Asset '{}' already has data loaded", assetObject->GetName());
    //
    //        return;
    //    }

    if (assetObject->IsTransient())
    {
        HYP_LOG(Assets, Warning, "Transient assets cannot be loaded from disk!");

        return;
    }

    HYP_LOG(Assets, Debug, "Loading asset '{}'", assetObject->GetName());

    if (Result result = Load_Internal(); result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", assetObject->GetName(), result.GetError().GetMessage());

        return;
    }

    HYP_LOG(Assets, Debug, "Successfully loaded asset '{}'", assetObject->GetName());
}

void AssetDataResourceBase::Destroy()
{
    HYP_SCOPE;

    AssetObject* assetObject = m_assetObject.GetUnsafe();

    HYP_LOG(Assets, Debug, "Unloading asset '{}'", assetObject->IsRegistered() ? *assetObject->GetPath().ToString() : *assetObject->GetName());

    if (!GetAssetRef().HasValue())
    {
        HYP_LOG(Assets, Warning, "Asset '{}' has no data to unload", assetObject->GetName());

        return;
    }

    Unload_Internal();
}

Result AssetDataResourceBase::Load_Internal()
{
    HYP_SCOPE;

    AssetObject* assetObject = m_assetObject.GetUnsafe();
    Assert(assetObject != nullptr);

    BufferedReader stream;

    HYP_DEFER({
        if (stream.GetSource() != nullptr)
        {
            delete stream.GetSource();
        }

        stream.Close();
    });

    if (Result openStreamResult = assetObject->OpenBinaryReadStream(stream); openStreamResult.HasError())
    {
        return openStreamResult;
    }

    if (Result loadResult = LoadFromStream(stream); loadResult.HasError())
    {
        return loadResult;
    }

    return {};
}

Result AssetDataResourceBase::Save_Internal(const FilePath& path)
{
    HYP_SCOPE;
    // mutex will already be locked by the asset object that owns this resource

    AssetObject* assetObject = m_assetObject.GetUnsafe();
    Assert(assetObject != nullptr);

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetAssetType().id);
    Assert(marshal != nullptr, "No marshal for asset type {}!", GetAssetType().name);

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

    Assert(object.GetType().GetNativeTypeId() == GetAssetType().id,
        "Object must have a native TypeId associated to be deserialized properly! Expected: {}, Got serialized type: {}",
        GetAssetType().name,
        object.GetType().ToString(true));

    writer.Append(std::move(object));

    FileByteWriter byteWriter { path };
    if (FBOMResult err = writer.Emit(&byteWriter))
    {
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk: {}", err.message);
    }

    HYP_LOG(Assets, Debug, "Saved asset to '{}'", path);

    return {};
}

#pragma endregion AssetResourceBase

#pragma region AssetObject

AssetObject::AssetObject()
    : m_resource(&GetNullResource()),
      m_flags(AssetObjectFlags::NONE),
      m_pool(nullptr)
{
}

AssetObject::AssetObject(Name name)
    : m_name(SanitizeName(name)),
      m_resource(&GetNullResource()),
      m_flags(AssetObjectFlags::NONE),
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
    HYP_SCOPE;
    ObjectBase::Init();

    if (m_resource && !m_resource->IsNull())
    {
        AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);
        resource->m_assetObject = WeakHandleFromThis();

        if ((m_flags[AssetObjectFlags::PERSISTENT] || DebugDisableUnload) && !m_persistentResource)
        {
            m_persistentResource = ResourceGuard(*m_resource);
        }
    }

    SetReady(true);
}

void AssetObject::SetIsPersistentlyLoaded(bool persistentlyLoaded, bool setFlag)
{
    HYP_SCOPE;

    if (setFlag)
    {
        m_flags[AssetObjectFlags::PERSISTENT] = persistentlyLoaded;
    }

    if (persistentlyLoaded)
    {
        if (!m_persistentResource && m_resource && !m_resource->IsNull())
        {
            m_persistentResource = ResourceGuard(*m_resource);
            Assert(m_persistentResource);
        }

        return;
    }

    if (DebugDisableUnload)
    {
        return;
    }

    m_persistentResource.Reset();
}

void AssetObject::SetIsTransient(bool isTransient)
{
    HYP_SCOPE;

    m_flags[AssetObjectFlags::TRANSIENT] = isTransient;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetIsPersistentlyLoaded(true, /* setFlag */ false);

        // transient assets don't have a manifest filepath as they are not saved to disk.
        m_manifestPath = FilePath();
    }
    else
    {
        SetIsPersistentlyLoaded(m_flags[AssetObjectFlags::PERSISTENT], /* setFlag */ false);
    }
}

void AssetObject::SetIsTransientByProxy(bool isTransientByProxy)
{
    HYP_SCOPE;

    m_flags[AssetObjectFlags::TRANSIENT_BY_PROXY] = isTransientByProxy;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetIsPersistentlyLoaded(true, /* setFlag */ false);

        // transient assets don't have a manifest filepath as they are not saved to disk.
        m_manifestPath = FilePath();
    }
    else
    {
        SetIsPersistentlyLoaded(m_flags[AssetObjectFlags::PERSISTENT], /* setFlag */ false);
    }
}

Result AssetObject::Rename(Name name)
{
    HYP_SCOPE;

    if (name == m_name)
    {
        // same name, do nothing
        return {};
    }

    name = SanitizeName(name);

    Handle<AssetPackage> package = GetPackage();

    if (package.IsValid())
    {
        Handle<AssetObject> strongThis = HandleFromThis();

        if (Result result = package->RemoveAssetObject(strongThis).Await(); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset object '{}' from package '{}': {}", m_name, package->GetName(), result.GetError().GetMessage());

            return result;
        }

        const Name prevName = m_name;
        m_name = name;

        if (Result result = package->AddAssetObject(strongThis).Await(); result.HasError())
        {
            m_name = prevName; // revert change

            HYP_LOG(Assets, Error, "Failed to rename asset object '{}' to '{}': {}", m_name, name, result.GetError().GetMessage());

            return result;
        }

        m_friendlyName = CreateFriendlyName(name);
    }
    else
    {
        m_name = name;
        m_friendlyName = CreateFriendlyName(name);
    }

    return {};
}

bool AssetObject::IsLoaded() const
{
    HYP_SCOPE;

    if (!m_resource || m_resource->IsNull())
    {
        return false;
    }

    return static_cast<AssetDataResourceBase*>(m_resource)->IsInitialized();
}

Result AssetObject::Save()
{
    HYP_SCOPE;

    // save our manifest first
    if (m_manifestPath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path is empty, cannot save");
    }

    AssertDebug(m_manifestPath.GetExtension() == "json", "Asset manifest path must have .json extension");

    const FilePath dir = m_manifestPath.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid directory, cannot save asset", dir);
    }

    FileByteWriter manifestWriter { m_manifestPath };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}', errno: {}", m_name, std::strerror(errno));
    }

    if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
    }

    manifestWriter.Close();

    const bool doSaveResource = m_resource != nullptr && !m_resource->IsNull();

    // use resource instead to save if it is not null
    if (doSaveResource)
    {
        AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);
        resource->IncRef();
        HYP_DEFER({ resource->DecRef(); });

        Mutex::Guard guard(resource->m_mutex);

        // get bin path from manifest path by removing .json extension
        const FilePath binPath = m_manifestPath.StripExtension();
        Assert(!binPath.Empty() && binPath != m_manifestPath);

        return resource->Save_Internal(binPath);
    }

#if 0 // just use manifest if no resource
    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetTypeId());
    Assert(marshal != nullptr);

    FBOMObject object;
    if (FBOMResult err = marshal->Serialize(ConstAnyRef(this), object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to serialize asset: {}", err.message);
    }

    writer.Append(std::move(object));

    FileByteWriter byteWriter { m_filepath };
    if (FBOMResult err = writer.Emit(&byteWriter))
    {
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk: {}", err.message);
    }
#endif

    HYP_LOG(Assets, Debug, "Saved asset manifest to '{}'", m_manifestPath);

    return {};
}

Result AssetObject::SaveManifest(ByteWriter& stream) const
{
    HYP_SCOPE;

    Json::JSObject manifestJson;

    ObjectToJSON(InstanceClass(), BoxedValue(HandleFromThis()), manifestJson, { .skipTransientProperties = true, .writeClassNames = true });

    stream.WriteString(Json::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

Result AssetObject::Load(
    BufferedReader& manifestStream,
    BufferedReader* binStream,
    Handle<AssetObject>& outAssetObject)
{
    HYP_SCOPE;

    if (!manifestStream.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Manifest stream is not open");
    }

    if (binStream && !binStream->IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Data stream given, but it is not open");
    }

    Json::ParseResult parseResult = Json::Parse(manifestStream);

    manifestStream.Close(); // not needed anymore

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    if (!parseResult.value.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest JSON must be an object, but got value: {}", parseResult.value.ToString());
    }

    Json::JSObject jsonObject = std::move(parseResult.value.AsObject());
    Json::Value classNameValue = jsonObject["$Class"];

    if (!classNameValue.IsString())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must contain a '$Class' string");
    }

    const Class* cls = GetClass(classNameValue.AsString().ToUtf8());

    if (!cls)
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' not found!", classNameValue.AsString());
    }

    if (!cls->IsDerivedFrom(AssetObject::StaticClass()))
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' is not derived from AssetObject!", classNameValue.AsString());
    }

    BoxedValue binData;

    if (binStream)
    {
        FBOMLoadContext context;
        FBOMReader reader { FBOMReaderConfig {} };
        FBOMResult err;

        if ((err = reader.Deserialize(context, *binStream, binData)))
        {
            return HYP_MAKE_ERROR(Error, "Failed to load asset: {}", err.message);
        }

        AssertDebug(binData.IsValid());
    }

    BoxedValue targetData;
    if (!cls->CreateInstance(targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create instance of class '{}'", classNameValue.AsString());
    }

    AssetObject* targetAssetObject = &targetData.Get<AssetObject>();
    const bool useResource = (targetAssetObject->m_resource != nullptr && !targetAssetObject->m_resource->IsNull());

    // remove class property
    jsonObject.Erase("$Class");

    if (!JSONToObject(jsonObject, cls, targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to deserialize asset object from manifest JSON");
    }

    AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(targetAssetObject->m_resource);

    if (useResource)
    {
        AssertDebug(resource != nullptr && !resource->IsNull());
        AssertDebug(binData.IsValid());

        if (binData.IsValid())
        {
            resource->Extract_Internal(binData.ToRef());
        }

        AssertDebug(resource->GetAssetRef().HasValue());
    }

    // invoke PostLoad callback
    targetAssetObject->InstanceClass()->PostLoad(targetAssetObject);

    outAssetObject = MakeStrongRef(targetAssetObject);

    return {};
}

Result AssetObject::OpenBinaryReadStream(BufferedReader& stream) const
{
    HYP_SCOPE;

    if (m_manifestPath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path is empty, cannot open read stream");
    }

    // get bin path from manifest path by removing .json extension
    AssertDebug(m_manifestPath.GetExtension() == "json", "Asset manifest path must have .json extension");

    const FilePath binPath = m_manifestPath.StripExtension();

    if (!binPath.Exists() || binPath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid file, cannot open read stream", binPath);
    }

    FileBufferedReaderSource* source = new FileBufferedReaderSource(binPath);

    stream = BufferedReader { source };

    if (!stream.IsOpen())
    {
        stream.Close();

        delete source;

        return HYP_MAKE_ERROR(Error, "Failed to open binary read stream for asset '{}'", m_name);
    }

    return {};
}

#pragma endregion AssetObject

} // namespace Hyperion
