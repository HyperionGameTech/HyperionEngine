/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/serialization/SerializationUtils.hpp>

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

    Assert(GetData() != nullptr);

    return {};
}

void AssetDataResourceBase::Initialize()
{
    HYP_SCOPE;

    Assert(m_assetObject != nullptr);

    if (IsDataLoaded())
    {
        HYP_LOG(Assets, Debug, "Asset '{}' already has data loaded in Initialize()", m_assetObject->GetName());
    
        return;
    }

    if (m_assetObject->IsTransient())
    {
        HYP_LOG(Assets, Warning, "Attempted to load transient asset {} from disk!", m_assetObject->GetPath().ToString());

        return;
    }

    HYP_LOG(Assets, Debug, "Loading asset '{}'", m_assetObject->GetName());

    if (Result result = Load_Internal(); result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", m_assetObject->GetName(), result.GetError().GetMessage());

        return;
    }

    HYP_LOG(Assets, Debug, "Successfully loaded asset '{}'", m_assetObject->GetName());
}

void AssetDataResourceBase::Destroy()
{
    HYP_SCOPE;

    AssertDebug(m_assetObject->IsSaved(),
        "Unloading asset data for asset that is not saved to disk, may cause problems later down the line");

    HYP_LOG(Assets, Debug, "Unloading asset '{}'", m_assetObject->IsRegistered() ? *m_assetObject->GetPath().ToString() : *m_assetObject->GetName());

    if (GetData() == nullptr)
    {
        HYP_LOG(Assets, Warning, "Asset '{}' has no data to unload", m_assetObject->GetName());

        return;
    }

    Unload_Internal();
}

Result AssetDataResourceBase::Load_Internal()
{
    HYP_SCOPE;

    Assert(m_assetObject != nullptr);

    BufferedReader stream;

    HYP_DEFER({
        if (stream.GetSource() != nullptr)
        {
            delete stream.GetSource();
        }

        stream.Close();
    });

    if (Result openStreamResult = m_assetObject->OpenBinaryReadStream(stream); openStreamResult.HasError())
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
    // mutex will already be locked by the asset object that owns this

    Assert(m_assetObject != nullptr);

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetDataTypeInfo().id);
    Assert(marshal != nullptr, "No marshal for asset data of type {}", GetDataTypeInfo().name);

    FBOMObject object;

    void* pData = GetData();

    if (!pData)
    {
        return HYP_MAKE_ERROR(Error, "Asset data reference is invalid!");
    }

    if (FBOMResult err = marshal->Serialize(ConstAnyRef(&GetDataTypeInfo(), pData), object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to serialize asset: {}", err.message);
    }

    Assert(object.GetType().GetNativeTypeId() == GetDataTypeInfo().id,
        "Object must have a native TypeId associated to be deserialized properly! Expected: {}, Got serialized type: {}",
        GetDataTypeInfo().name,
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
    : m_resource(nullptr),
      m_flags(AssetObjectFlags::NONE),
      m_isDirty(0)
{
}

AssetObject::AssetObject(Name name)
    : m_name(SanitizeName(name)),
      m_resource(nullptr),
      m_flags(AssetObjectFlags::NONE),
      m_isDirty(0)
{
}

AssetObject::~AssetObject()
{
    // need to release before freeing resource or we'll deadlock
    m_persistentResource.Release();

    if (m_resource != nullptr)
    {
        PoolDelete(*g_assetPool, m_resource);
        m_resource = nullptr;
    }
}

void AssetObject::Init()
{
    HYP_SCOPE;
    ObjectBase::Init();

    if (m_resource)
    {
        AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);
        resource->m_assetObject = this;

        if ((m_flags[AssetObjectFlags::PERSISTENT] || DebugDisableUnload) && !m_persistentResource)
        {
            m_persistentResource = m_resource->GetReadScope();
            Assert(m_persistentResource);
        }
    }

    SetReady(true);
}

void AssetObject::SetAssetFlags(EnumFlags<AssetObjectFlags> flags)
{
    if (m_flags != flags)
    {
        const bool wasPersistent = m_flags[AssetObjectFlags::PERSISTENT];

        m_flags = flags;

        const bool isPersistent = m_flags[AssetObjectFlags::PERSISTENT];

        if (wasPersistent != isPersistent)
        {
            SetPersistentRequested(isPersistent, /* setFlag */ false);
        }

        MarkDirty();
    }
}

void AssetObject::MarkDirty()
{
    int32 expected = 0;
    if (AtomicCompareExchange(&m_isDirty, expected, 1))
    {
        OnDirtyStateChanged(true);
    }
}

void AssetObject::SetPersistentRequested(bool persistentlyLoaded, bool setFlag, bool markDirty)
{
    HYP_SCOPE;

    if (setFlag && m_flags[AssetObjectFlags::PERSISTENT] != persistentlyLoaded)
    {
        if (markDirty)
        {
            MarkDirty();
        }

        m_flags[AssetObjectFlags::PERSISTENT] = persistentlyLoaded;
    }

    if (persistentlyLoaded)
    {
        if (!m_persistentResource && m_resource)
        {
            m_persistentResource = m_resource->GetReadScope();
            Assert(m_persistentResource);
        }

        return;
    }

    if (DebugDisableUnload)
    {
        return;
    }

    // if transient, we need to keep it in memory.
    // we also keep it in memory if `setFlag` was false and the PERSISTENT flag is set (it overrides it)
    if (!persistentlyLoaded && !m_flags[AssetObjectFlags::PERSISTENT] && !IsTransient())
    {
        m_persistentResource.Release();
    }
}

void AssetObject::SetIsTransient(bool isTransient)
{
    HYP_SCOPE;

    m_flags[AssetObjectFlags::TRANSIENT] = isTransient;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetPersistentRequested(true, /* setFlag */ false);

        // transient assets don't have a manifest filepath as they are not saved to disk.
        m_manifestPath = FilePath();
    }
    else
    {
        SetPersistentRequested(false, /* setFlag */ false);
    }
}

void AssetObject::SetIsTransientByProxy(bool isTransientByProxy)
{
    HYP_SCOPE;

    m_flags[AssetObjectFlags::TRANSIENT_BY_PROXY] = isTransientByProxy;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetPersistentRequested(true, /* setFlag */ false);

        // transient assets don't have a manifest filepath as they are not saved to disk.
        m_manifestPath = FilePath();
    }
    else
    {
        SetPersistentRequested(false, /* setFlag */ false);
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

        if (Result result = package->RemoveAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset object '{}' from package '{}': {}", m_name, package->GetName(), result.GetError().GetMessage());

            return result;
        }

        const Name prevName = m_name;
        m_name = name;

        if (Result result = package->AddAssetObject(strongThis, /* replaceOnConflict */ false); result.HasError())
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

    MarkDirty();

    return {};
}

bool AssetObject::IsDataLoaded() const
{
    HYP_SCOPE;

    return m_resource != nullptr
        && static_cast<AssetDataResourceBase*>(m_resource)->IsDataLoaded();
}

bool AssetObject::IsSaved() const
{
    return m_manifestPath.Length() > 0;
}

Result AssetObject::Save(const FilePath& manifestPath)
{
    HYP_SCOPE;

    // save our manifest first
    if (manifestPath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path is empty, cannot save");
    }

    if (manifestPath.GetExtension() != "json")
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path must have .json extension");
    }

    const FilePath dir = manifestPath.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid directory, cannot save asset", dir);
    }

    {
        FileByteWriter manifestWriter { manifestPath };

        if (!manifestWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}', errno: {}", m_name, std::strerror(errno));
        }

        if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to save manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
        }

        manifestWriter.Close();
    }

    const bool saveBinData = m_resource != nullptr;

    // use resource instead to save if it is not null
    if (saveBinData)
    {
        AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(m_resource);

        // must load before saving if saving to a different place and not currently in memory.
        bool requiresLoad = !resource->IsDataLoaded();

        ResourceGuard resGuard;

        if (requiresLoad)
        {
            requiresLoad = false;

            Assert(IsSaved(), "Cannot load asset {} from disk; no manifest path for the asset.",
                m_name);

            resGuard = resource->GetReadScope();

            if (!resource->IsDataLoaded())
            {
                return HYP_MAKE_ERROR(Error, "Asset with manifest at path {} has no data, cannot save!", manifestPath);
            }
        }

        // get bin path from manifest path by removing .json extension
        const FilePath binPath = manifestPath.StripExtension();
        Assert(!binPath.Empty() && binPath != manifestPath);

        if (Result saveResourceResult = resource->Save_Internal(binPath); saveResourceResult.HasError())
        {
            return saveResourceResult.GetError();
        }
    }

    HYP_LOG(Assets, Debug, "Saved asset manifest to '{}'", manifestPath);
    
    // need to set manifest path after saving the resource, because if we need to load the data first in order
    // to save it somewhere else, we'll need the previous manifest path to still exist otherwise we'll try to load
    // something that doesn't exist.
    m_manifestPath = manifestPath;

    // no longer dirty
    AtomicExchange(&m_isDirty, 0);

    OnDirtyStateChanged(false);

    return {};
}

Result AssetObject::SaveManifest(ByteWriter& stream) const
{
    HYP_SCOPE;

    JSON::Object manifestJson;

    ToJSONOptions opts;
    opts.skipTransientProperties = true;
    opts.writeClassNames = true;

    ObjectToJSON(InstanceClass(), BoxedValue(HandleFromThis()), manifestJson, opts);

    stream.WriteString(JSON::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

HYP_DISABLE_OPTIMIZATION;

Result AssetObject::Load(
    JSON::Object& manifestData,
    BufferedReader* binStream,
    Handle<AssetObject>& outAssetObject)
{
    HYP_SCOPE;

    if (binStream && !binStream->IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Data stream given, but it is not open");
    }

    JSON::Value classNameValue = manifestData["$Class"];

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

    BoxedValue binDataBoxed;

    if (binStream)
    {
        FBOMLoadContext context;
        FBOMReader reader { FBOMReaderConfig {} };
        FBOMResult err;

        if ((err = reader.Deserialize(context, *binStream, binDataBoxed)))
        {
            return HYP_MAKE_ERROR(Error, "Failed to load asset: {}", err.message);
        }

        AssertDebug(binDataBoxed.IsValid());
    }

    BoxedValue targetData;
    if (!cls->CreateInstance(targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create instance of class '{}'", classNameValue.AsString());
    }

    AssetObject* targetAssetObject = &targetData.Get<AssetObject>();
    const bool loadBinData = targetAssetObject->m_resource != nullptr;

    // remove class property
    manifestData.Erase("$Class");

    if (!ObjectFromJSON(manifestData, cls, targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to deserialize asset object from manifest JSON");
    }

    AssetDataResourceBase* resource = static_cast<AssetDataResourceBase*>(targetAssetObject->m_resource);

    if (loadBinData)
    {
        AssertDebug(resource != nullptr);
        AssertDebug(binDataBoxed.IsValid());

        if (binDataBoxed.IsValid())
        {
            resource->Extract_Internal(binDataBoxed.ToRef());
        }

        AssertDebug(resource->GetData() != nullptr);
    }

    outAssetObject = MakeStrongRef(targetAssetObject);

    return {};
}

Result AssetObject::OpenBinaryReadStream(BufferedReader& stream) const
{
    HYP_SCOPE;

    if (m_manifestPath.Empty())
    {
        HYP_BREAKPOINT_DEBUG_MODE;

        return HYP_MAKE_ERROR(Error, "Asset manifest path is empty, cannot open read stream");
    }

    // get bin path from manifest path by removing .json extension
    if (m_manifestPath.GetExtension() != "json")
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path must have .json extension");
    }

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
