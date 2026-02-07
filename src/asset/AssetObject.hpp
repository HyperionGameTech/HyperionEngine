/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Constants.hpp>
#include <core/Defines.hpp>

namespace Hyperion {

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

namespace JSON {
class Object;
} // namespace JSON

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
    AssetDataResourceBase()
        : m_assetObject(nullptr),
          m_hasData(false)
    {
    }

    virtual void Initialize() override final;
    virtual void Destroy() override final;

    Result Load_Internal();
    Result Save_Internal(const FilePath& path);

    virtual void Unload_Internal() = 0;

    virtual void Extract_Internal(AnyRef ref) = 0;

    HYP_FORCE_INLINE bool IsDataLoaded() const
    {
        return m_hasData;
    }

    virtual const TypeInfo& GetDataTypeInfo() const = 0;

    virtual void* GetData() = 0;

    AssetObject* m_assetObject;
    bool m_hasData;
};

template <class T>
class AssetDataResource final : public AssetDataResourceBase
{
public:
    AssetDataResource() = default;

    AssetDataResource(const T& data)
    {
        m_dataStorage.Construct(data);
        m_hasData = true;
    }

    AssetDataResource(T&& data)
    {
        m_dataStorage.Construct(std::move(data));
        m_hasData = true;
    }

    AssetDataResource(const AssetDataResource&) = delete;
    AssetDataResource& operator=(const AssetDataResource&) = delete;

    AssetDataResource(AssetDataResource&&) noexcept = delete;
    AssetDataResource& operator=(AssetDataResource&&) noexcept = delete;

    virtual ~AssetDataResource() override
    {
        if (m_hasData)
        {
            m_hasData = false;
            m_dataStorage.Destruct();
        }
    }

    virtual const TypeInfo& GetDataTypeInfo() const override
    {
        return TypeOf<NormalizedType<T>>();
    }

    virtual void* GetData() override
    {
        return m_hasData ? m_dataStorage.GetPointer() : nullptr;
    }

protected:
    virtual void Unload_Internal() override
    {
        if (m_hasData)
        {
            m_hasData = false;

            m_dataStorage.Destruct();
        }
    }

    virtual void Extract_Internal(AnyRef ref) override
    {
        if (m_hasData)
        {
            m_dataStorage.Get() = ref.Get<T>();
        }
        else
        {
            m_dataStorage.Construct(ref.Get<T>());

            m_hasData = true;
        }
    }

    ValueStorage<T> m_dataStorage;
};

HYP_ENUM()
enum class AssetObjectFlags : uint32
{
    NONE = 0x0,
    PERSISTENT = 0x1,        //!< Asset is persistently loaded in memory
    TRANSIENT = 0x2,         //!< Asset is not saved to disk
    TRANSIENT_BY_PROXY = 0x4 //!< Same as above, but is transient due to parent package being transient (will change if asset is moved to a non-transient package)
};

HYP_MAKE_ENUM_FLAGS(AssetObjectFlags);

HYP_CLASS(Abstract)
class HYP_API AssetObject : public ObjectBase
{
    HYP_OBJECT_BODY(AssetObject);

protected:
    template <class T>
    void SetData(T&& data)
    {
        if (m_resource)
        {
            PoolDelete(*g_assetPool, m_resource);
        }

        m_resource = PoolNew<AssetDataResource<NormalizedType<T>>>(*g_assetPool, std::forward<T>(data));
        m_resource->m_assetObject = this;
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

    HYP_METHOD(Property = "Name", Serialize, EditOrder = 1)
    Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name")
    HYP_FORCE_INLINE void SetName(Name name)
    {
        (void)Rename(name);
    }

    HYP_METHOD()
    virtual Result Rename(Name name);

    HYP_METHOD()
    bool IsDirty() const
    {
        return AtomicAdd(&m_isDirty, 0) != 0;
    }

    HYP_METHOD()
    void MarkDirty();

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

    HYP_FORCE_INLINE AssetDataResourceBase* GetResource() const
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

    HYP_METHOD(Property = "AssetFlags", Serialize)
    EnumFlags<AssetObjectFlags> GetAssetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD(Property = "AssetFlags")
    void SetAssetFlags(EnumFlags<AssetObjectFlags> flags);

    HYP_METHOD()
    bool IsPersistent() const
    {
        return bool(m_persistentResource);
    }

    HYP_METHOD()
    void SetPersistentRequested(bool persistentlyLoaded, bool setFlag = true, bool markDirty = true);

    HYP_METHOD()
    bool IsTransient() const
    {
        return bool(m_flags & (AssetObjectFlags::TRANSIENT | AssetObjectFlags::TRANSIENT_BY_PROXY));
    }

    HYP_METHOD()
    bool IsTransientByProxy() const
    {
        return (m_flags & (AssetObjectFlags::TRANSIENT | AssetObjectFlags::TRANSIENT_BY_PROXY)) == AssetObjectFlags::TRANSIENT_BY_PROXY;
    }

    HYP_METHOD()
    void SetIsTransient(bool isTransient);

    HYP_METHOD()
    void SetIsTransientByProxy(bool isTransientByProxy);

    HYP_METHOD()
    bool IsDataLoaded() const;

    HYP_METHOD()
    bool IsSaved() const;

    HYP_METHOD()
    Result Save(const FilePath& manifestPath);

    /*! \brief Opens a read stream for the binary data of this asset.
     *  \param stream The stream to open.
     *  \return Result indicating success or failure of the operation. */
    Result OpenBinaryReadStream(BufferedReader& stream) const;

    static Result Load(
        JSON::Object& manifestData,
        BufferedReader* binStream, // optional
        Handle<AssetObject>& outAssetObject);

protected:
    void Init() override;

    virtual void OnDirtyStateChanged(bool isDirty)
    {
        // do nothing by default
    }

    Result SaveManifest(ByteWriter& stream) const;

    template <class T>
    T* GetResourceData() const
    {
        static_assert(std::is_same_v<T, NormalizedType<T>>);

        if (!m_resource)
        {
            return nullptr;
        }

        AssetDataResourceBase* resourceCastedBase = static_cast<AssetDataResourceBase*>(m_resource);

        const bool isExpectedType = TypeInfo_GetId(resourceCastedBase->GetDataTypeInfo()) == TypeIdOf<T>();

        AssertDebug(isExpectedType, "Type mismatch! Expected: {} but got: {}",
            TypeInfo_GetName(resourceCastedBase->GetDataTypeInfo()),
            TypeInfo_GetName(TypeOf<T>()));

        if (!isExpectedType)
        {
            return nullptr;
        }
        
        AssetDataResource<T>* resourceCasted = static_cast<AssetDataResource<T>*>(m_resource);
        AssertDebug(resourceCasted->GetData() != nullptr);

        return static_cast<T*>(resourceCasted->GetData());
    }

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "FriendlyName")
    Name m_friendlyName;

    HYP_FIELD(Property = "AssetFlags")
    EnumFlags<AssetObjectFlags> m_flags;

    HYP_FIELD()
    FilePath m_originalFilepath; // used to determine if we should skip importing an asset

    HYP_FIELD(Transient)
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD(NoScriptBindings, Transient)
    AssetDataResourceBase* m_resource;

    HYP_FIELD(Transient)
    AssetPath m_assetPath;

    HYP_FIELD(Transient)
    FilePath m_manifestPath;

    HYP_FIELD(NoScriptBindings, Transient)
    ResourceGuard m_persistentResource;

    HYP_FIELD(NoScriptBindings, Transient)
    mutable volatile int32 m_isDirty;
};

} // namespace Hyperion
