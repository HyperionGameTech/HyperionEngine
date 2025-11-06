/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Constants.hpp>
#include <core/Defines.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetPackage;
class AssetObject;
class ByteWriter;

class BufferedReader;
using BufferedByteReader = BufferedReader;

namespace functional {

template <class FunctionSignature>
class ProcRef;

} // namespace functional

using functional::ProcRef;

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class HYP_API AssetDataResourceBase : public ResourceBase
{
public:
    friend class AssetObject;

    AssetDataResourceBase(const AssetDataResourceBase&) = delete;
    AssetDataResourceBase& operator=(const AssetDataResourceBase&) = delete;

    AssetDataResourceBase(AssetDataResourceBase&&) noexcept = delete;
    AssetDataResourceBase& operator=(AssetDataResourceBase&&) noexcept = delete;

    virtual ~AssetDataResourceBase() override = default;

    /*! \brief Initialize the resource data from the given stream.
     *  \param stream The stream to read from.
     *  \return Result indicating success or failure of the operation. */
    Result LoadFromStream(BufferedReader& stream);

protected:
    AssetDataResourceBase() = default;

    virtual void Initialize() override final;
    virtual void Destroy() override final;

    Result Load_Internal();
    Result Save_Internal(const FilePath& path);

    virtual void Unload_Internal() = 0;

    virtual void Extract_Internal(AnyRef ref) = 0;

    virtual bool IsDataLoaded() const = 0;

    virtual const TypeInfo& GetAssetType() const = 0;
    virtual AnyRef GetAssetRef() = 0;

    WeakHandle<AssetObject> m_assetObject;
    mutable Mutex m_mutex;
};

template <class T>
class AssetDataResource final : public AssetDataResourceBase
{
public:
    AssetDataResource()
        : m_data(nullptr)
    {
    }

    AssetDataResource(const T& data)
        : m_data(PoolNew<T>(*g_assetPool, data))
    {
    }

    AssetDataResource(T&& data)
        : m_data(PoolNew<T>(*g_assetPool, std::move(data)))
    {
    }

    AssetDataResource(const AssetDataResource&) = delete;
    AssetDataResource& operator=(const AssetDataResource&) = delete;

    AssetDataResource(AssetDataResource&&) noexcept = delete;
    AssetDataResource& operator=(AssetDataResource&&) noexcept = delete;

    virtual ~AssetDataResource() override
    {
        if (m_data != nullptr)
        {
            PoolDelete(*g_assetPool, m_data);
            m_data = nullptr;
        }
    }

    virtual bool IsDataLoaded() const override
    {
        return m_data != nullptr;
    }

protected:
    virtual void Unload_Internal() override
    {
        if (m_data)
        {
            PoolDelete(*g_assetPool, m_data);
            m_data = nullptr;
        }
    }

    virtual void Extract_Internal(AnyRef ref) override
    {
        if (m_data != nullptr)
        {
            *m_data = ref.Get<T>();
        }
        else
        {
            m_data = PoolNew<T>(*g_assetPool, ref.Get<T>());
        }
    }

    virtual const TypeInfo& GetAssetType() const override
    {
        AnyRef assetRef = const_cast<AssetDataResource*>(this)->GetAssetRef();
        if (!assetRef)
        {
            return TypeInfo_Void();
        }

        return *assetRef.GetTypeInfo();
    }

    virtual AnyRef GetAssetRef() override
    {
        return AnyRef(m_data);
    }

    T* m_data;
};

HYP_ENUM()
enum AssetObjectFlags : uint32
{
    AOF_NONE = 0x0,
    AOF_PERSISTENT = 0x1,        //!< Asset is persistently loaded in memory
    AOF_TRANSIENT = 0x2,         //!< Asset is not saved to disk
    AOF_TRANSIENT_BY_PROXY = 0x4 //!< Same as above, but is transient due to parent package being transient (will change if asset is moved to a non-transient package)
};

HYP_MAKE_ENUM_FLAGS(AssetObjectFlags);

HYP_CLASS(Abstract)
class HYP_API AssetObject : public ObjectBase
{
    HYP_OBJECT_BODY(AssetObject);

    template <class T>
    static ResourceMemoryPool<AssetDataResource<NormalizedType<T>>>& GetPool()
    {
        static ResourceMemoryPool<AssetDataResource<NormalizedType<T>>>* pool = ResourceMemoryPool<AssetDataResource<NormalizedType<T>>>::GetInstance();
        return *pool;
    }

protected:
    template <class T>
    void SetData(T&& data)
    {
        static ResourceMemoryPool<AssetDataResource<NormalizedType<T>>>& pool = GetPool<T>();

        m_pool = &pool;

        m_resource = pool.Allocate(std::forward<T>(data));
        static_cast<AssetDataResource<NormalizedType<T>>*>(m_resource)->m_assetObject = WeakHandleFromThis();
    }

public:
    friend class AssetRegistry;
    friend class AssetPackage;

    AssetObject();
    explicit AssetObject(Name name);

    template <class T>
    AssetObject(Name name, T&& data)
        : AssetObject(name)
    {
        AssetObject::SetData(std::forward<T>(data));
    }

    AssetObject(const AssetObject& other) = delete;
    AssetObject& operator=(const AssetObject& other) = delete;

    AssetObject(AssetObject&& other) noexcept = delete;
    AssetObject& operator=(AssetObject&& other) noexcept = delete;

    virtual ~AssetObject();

    HYP_METHOD()
    const Uuid& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE void SetName(Name name)
    {
        (void)Rename(name);
    }

    HYP_METHOD()
    virtual Result Rename(Name name);

    HYP_METHOD(Property = "FriendlyName")
    Name GetFriendlyName() const
    {
        return m_friendlyName.IsValid() ? m_friendlyName : m_name;
    }

    HYP_METHOD(Property = "FriendlyName")
    void SetFriendlyName(Name friendlyName)
    {
        m_friendlyName = friendlyName;
    }

    HYP_METHOD()
    const FilePath& GetOriginalFilepath() const
    {
        return m_originalFilepath;
    }

    HYP_METHOD()
    void SetOriginalFilepath(const FilePath& originalFilepath)
    {
        m_originalFilepath = originalFilepath;
    }

    HYP_METHOD()
    Handle<AssetPackage> GetPackage() const
    {
        return m_package.Lock();
    }

    HYP_FORCE_INLINE IResource* GetResource() const
    {
        return m_resource;
    }

    HYP_METHOD()
    const AssetPath& GetPath() const
    {
        AssertDebug(IsRegistered(), "Calling GetPath() on an unregistered asset object");

        return m_assetPath;
    }

    HYP_METHOD()
    bool IsRegistered() const
    {
        return m_package.IsValid();
    }

    HYP_METHOD()
    EnumFlags<AssetObjectFlags> GetAssetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    void SetAssetFlags(EnumFlags<AssetObjectFlags> flags)
    {
        const bool wasPersistent = m_flags[AOF_PERSISTENT];

        m_flags = flags;

        const bool isPersistent = m_flags[AOF_PERSISTENT];

        if (wasPersistent != isPersistent)
        {
            SetIsPersistentlyLoaded(isPersistent, /* setFlag */ false);
        }
    }

    HYP_METHOD()
    bool IsPersistentlyLoaded() const
    {
        return bool(m_persistentResource);
    }

    HYP_METHOD()
    void SetIsPersistentlyLoaded(bool persistentlyLoaded, bool setFlag = true);

    HYP_METHOD()
    bool IsTransient() const
    {
        return bool(m_flags & (AOF_TRANSIENT | AOF_TRANSIENT_BY_PROXY));
    }

    HYP_METHOD()
    void SetIsTransient(bool isTransient);

    HYP_METHOD()
    void SetIsTransientByProxy(bool isTransientByProxy);

    HYP_METHOD()
    bool IsLoaded() const;

    HYP_METHOD()
    Result Save();

    /*! \brief Opens a read stream for the binary data of this asset.
     *  \param stream The stream to open.
     *  \return Result indicating success or failure of the operation. */
    Result OpenBinaryReadStream(BufferedReader& stream) const;

    static Result Load(
        BufferedReader& manifestStream,
        BufferedReader* pBinStream, // optional
        Handle<AssetObject>& outAssetObject);

protected:
    void Init() override;

    Result SaveManifest(ByteWriter& stream) const;

    template <class T>
    T* GetResourceData() const
    {
        if (!m_resource || m_resource->IsNull())
        {
            return nullptr;
        }

        AssetDataResourceBase* resourceCasted = static_cast<AssetDataResourceBase*>(m_resource);
        AssertDebug(TypeInfo_GetId(resourceCasted->GetAssetType()) == TypeInfo_GetId(TypeOf<T>()), "Type mismatch!");

        return resourceCasted->GetAssetRef().TryGet<T>();
    }

    HYP_FIELD()
    Uuid m_uuid;

    HYP_FIELD()
    Name m_name;

    HYP_FIELD(Property = "FriendlyName")
    Name m_friendlyName;

    HYP_FIELD()
    EnumFlags<AssetObjectFlags> m_flags;

    HYP_FIELD()
    FilePath m_originalFilepath; // used to determine if we should skip importing an asset

    HYP_FIELD(Transient)
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD(NoScriptBindings, Transient)
    IResource* m_resource;

    HYP_FIELD(Transient)
    AssetPath m_assetPath;

    HYP_FIELD(Transient)
    FilePath m_manifestPath;

    HYP_FIELD(NoScriptBindings, Transient)
    IResourceMemoryPool* m_pool;

    HYP_FIELD(NoScriptBindings, Transient)
    ResourceHandle m_persistentResource;
};

} // namespace hyperion
