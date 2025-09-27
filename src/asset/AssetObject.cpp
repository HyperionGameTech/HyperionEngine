/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/object/HypDataJSONHelpers.hpp>
#include <core/object/HypClass.hpp>

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

extern HYP_API const FilePath& GetResourceDirectory();

extern HYP_NODISCARD String SanitizeName(const UTF8StringView& nameStr);
extern HYP_NODISCARD Name SanitizeName(Name name);
extern HYP_NODISCARD Name CreateFriendlyName(Name name);

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

    if (Result result = Load_Internal(); result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", assetObject->GetName(), result.GetError().GetMessage());
    }
}

void AssetDataResourceBase::Destroy()
{
    HYP_LOG(Assets, Debug, "Unloading asset '{}'", m_assetObject.GetUnsafe()->IsRegistered() ? *m_assetObject.GetUnsafe()->GetPath().ToString() : *m_assetObject.GetUnsafe()->GetName());

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

    if (Result openStreamResult = assetObject->OpenReadStream(stream); openStreamResult.HasError())
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
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk: {}", err.message);
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
    : m_name(SanitizeName(name)),
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

    name = SanitizeName(name);

    Handle<AssetPackage> package = GetPackage();

    if (package.IsValid())
    {
        Handle<AssetObject> strongThis = HandleFromThis();

        if (Result result = package->RemoveAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset object '{}' from package '{}': {}", m_name, package->GetName(), result.GetError().GetMessage());

            return result;
        }

        const Name prevName = m_name;
        m_name = name;

        if (Result result = package->AddAssetObject(strongThis); result.HasError())
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
    if (!m_resource || m_resource->IsNull())
    {
        return false;
    }

    return static_cast<AssetDataResourceBase*>(m_resource)->IsInitialized();
}

Result AssetObject::Save()
{
    if (!m_resource || m_resource->IsNull())
    {
        return HYP_MAKE_ERROR(Error, "No resource set, cannot save");
    }

    AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);
    resource->IncRef();
    HYP_DEFER({ resource->DecRef(); });

    Mutex::Guard guard(resource->m_mutex);

    if (m_filepath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset path is empty, cannot save");
    }

    const FilePath dir = m_filepath.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid directory, cannot save asset", dir);
    }

    FileByteWriter manifestWriter { m_filepath + ".json" };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}', errno: {}", m_name, std::strerror(errno));
    }

    if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
    }

    manifestWriter.Close();

    return resource->Save_Internal(m_filepath);
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
    if (m_filepath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset path is empty, cannot open read stream");
    }

    FileBufferedReaderSource* source = new FileBufferedReaderSource(m_filepath);

    stream = BufferedReader { source };

    if (!stream.IsOpen())
    {
        stream.Close();

        delete source;

        return HYP_MAKE_ERROR(Error, "Failed to open stream for asset '{}'", m_name);
    }

    return {};
}

#pragma endregion AssetObject

} // namespace hyperion
