/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Constants.hpp>
#include <core/Defines.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetPackage;
class AssetObject;

class AssetDataResourceBase : public ResourceBase
{
public:
    friend class AssetObject;

    AssetDataResourceBase(const AssetDataResourceBase&) = delete;
    AssetDataResourceBase& operator=(const AssetDataResourceBase&) = delete;

    AssetDataResourceBase(AssetDataResourceBase&&) noexcept = delete;
    AssetDataResourceBase& operator=(AssetDataResourceBase&&) noexcept = delete;

    ~AssetDataResourceBase() = default;

    /*! \brief Initialize the resource data from the given stream.
     *  \param stream The stream to read from.
     *  \return Result indicating success or failure of the operation. */
    Result LoadFromStream(BufferedReader& stream);

protected:
    AssetDataResourceBase()
    {
    }

    virtual void Initialize() override final;
    virtual void Destroy() override final;

    Result Load_Internal();
    Result Save_Internal(const FilePath& path);

    virtual void Unload_Internal() = 0;

    virtual void Extract_Internal(HypData&& data) = 0;

    virtual TypeId GetAssetTypeId() const = 0;
    virtual AnyRef GetAssetRef() = 0;

    WeakHandle<AssetObject> m_assetObject;
    mutable Mutex m_mutex;
};

template <class T>
class AssetDataResource final : public AssetDataResourceBase
{
public:
    AssetDataResource() = default;

    AssetDataResource(const T& data)
        : m_data(data)
    {
    }

    AssetDataResource(T&& data)
        : m_data(std::move(data))
    {
    }

    AssetDataResource(const AssetDataResource&) = delete;
    AssetDataResource& operator=(const AssetDataResource&) = delete;

    AssetDataResource(AssetDataResource&&) noexcept = delete;
    AssetDataResource& operator=(AssetDataResource&&) noexcept = delete;

    virtual ~AssetDataResource() override = default;

protected:
    virtual void Unload_Internal() override
    {
        m_data = {};
    }

    virtual void Extract_Internal(HypData&& data) override
    {
        m_data = std::move(data.Get<T>());
    }

    virtual TypeId GetAssetTypeId() const override
    {
        return const_cast<AssetDataResource*>(this)->GetAssetRef().GetTypeId();
    }

    virtual AnyRef GetAssetRef() override
    {
        return AnyRef(&m_data);
    }

    T m_data;
};

HYP_ENUM()
enum AssetObjectFlags : uint32
{
    AOF_NONE = 0x0,
    AOF_PERSISTENT = 0x1 //!< Asset is persistently loaded in memory
};

HYP_MAKE_ENUM_FLAGS(AssetObjectFlags);

HYP_CLASS(Abstract)
class HYP_API AssetObject : public HypObjectBase
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
    HYP_FORCE_INLINE const Uuid& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE void SetName(Name name)
    {
        (void)Rename(name);
    }

    HYP_METHOD()
    virtual Result Rename(Name name);

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetFriendlyName() const
    {
        return m_friendlyName.IsValid() ? m_friendlyName : m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetFriendlyName(Name friendlyName)
    {
        m_friendlyName = friendlyName;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const FilePath& GetOriginalFilepath() const
    {
        return m_originalFilepath;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetOriginalFilepath(const FilePath& originalFilepath)
    {
        m_originalFilepath = originalFilepath;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Handle<AssetPackage> GetPackage() const
    {
        return m_package.Lock();
    }

    HYP_FORCE_INLINE IResource* GetResource() const
    {
        return m_resource;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const AssetPath& GetPath() const
    {
        AssertDebug(IsRegistered(), "Calling GetPath() on an unregistered asset object");

        return m_assetPath;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsRegistered() const
    {
        return m_package.IsValid();
    }

    HYP_METHOD()
    HYP_FORCE_INLINE EnumFlags<AssetObjectFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsPersistentlyLoaded() const
    {
        return bool(m_persistentResource);
    }

    HYP_METHOD()
    void SetIsPersistentlyLoaded(bool persistentlyLoaded);

    HYP_METHOD()
    bool IsLoaded() const;

    HYP_METHOD()
    Result Save();

    Result OpenReadStream(BufferedReader& stream) const;

    static Result Load(
        BufferedReader& manifestStream,
        BufferedReader& dataStream,
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
        AssertDebug(resourceCasted->GetAssetTypeId() == TypeId::ForType<T>(), "Type mismatch!");

        return resourceCasted->GetAssetRef().TryGet<T>();
    }

    HYP_FIELD(Serialize)
    Uuid m_uuid;

    HYP_FIELD(Serialize)
    Name m_name;

    HYP_FIELD(Serialize)
    Name m_friendlyName;

    HYP_FIELD(Serialize)
    EnumFlags<AssetObjectFlags> m_flags;

    HYP_FIELD(Serialize)
    FilePath m_originalFilepath; // used to determine if we should skip importing an asset

    HYP_FIELD(JsonIgnore)
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD(JsonIgnore, NoScriptBindings)
    IResource* m_resource;

    HYP_FIELD(JsonIgnore)
    AssetPath m_assetPath;

    FilePath m_filepath;
    IResourceMemoryPool* m_pool;
    ResourceHandle m_persistentResource;
};

} // namespace hyperion
