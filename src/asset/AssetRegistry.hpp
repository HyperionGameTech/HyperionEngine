/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/StringView.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/Defines.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/Constants.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetRegistry;
class AssetPackage;
class AssetObject;

HYP_API extern WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage);
HYP_API extern WeakName AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject);

using AssetPackageSet = HashSet<Handle<AssetPackage>, &AssetPackage_KeyByFunction>;
using AssetObjectSet = HashSet<Handle<AssetObject>, &AssetObject_KeyByFunction>;

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

    ~AssetObject();

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

    HYP_METHOD()
    Result Rename(Name name);

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
    Result Save() const;

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

HYP_ENUM()
enum AssetPackageFlags : uint32
{
    APF_NONE = 0x0,
    APF_TRANSIENT = 0x1,
    APF_HIDDEN = 0x2
};

HYP_MAKE_ENUM_FLAGS(AssetPackageFlags);

class AssetPackageValidationError final : public Error
{
public:
    enum ErrorCode : int
    {
        UNKNOWN = -1,
        ERR_CYCLIC_DEPENDENCY
    };

    AssetPackageValidationError()
        : Error(),
          m_errorCode(UNKNOWN)
    {
    }

    template <auto MessageString>
    AssetPackageValidationError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, ErrorCode errorCode)
        : Error(currentFunction, ValueWrapper<MessageString>()),
          m_errorCode(errorCode)
    {
    }

    template <auto MessageString, class... Args>
    AssetPackageValidationError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(UNKNOWN)
    {
    }

    virtual ~AssetPackageValidationError() override = default;

    HYP_FORCE_INLINE ErrorCode GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    ErrorCode m_errorCode;
};

using AssetPackageValidationResult = TResult<void, AssetPackageValidationError>;


HYP_CLASS()
class HYP_API AssetPackage final : public HypObjectBase
{
    HYP_OBJECT_BODY(AssetPackage);

    friend class AssetRegistry;

public:
    AssetPackage();
    AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags = APF_NONE);
    AssetPackage(const AssetPackage& other) = delete;
    AssetPackage& operator=(const AssetPackage& other) = delete;
    AssetPackage(AssetPackage&& other) noexcept = delete;
    AssetPackage& operator=(AssetPackage&& other) noexcept = delete;
    ~AssetPackage() = default;

    HYP_METHOD()
    HYP_FORCE_INLINE const Uuid& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetUUID(const Uuid& uuid)
    {
        m_uuid = uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

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
    HYP_FORCE_INLINE EnumFlags<AssetPackageFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsTransient() const
    {
        return m_flags[APF_TRANSIENT];
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHidden() const
    {
        return m_flags[APF_HIDDEN];
    }

    HYP_FORCE_INLINE const WeakHandle<AssetRegistry>& GetRegistry() const
    {
        return m_registry;
    }

    HYP_FORCE_INLINE const WeakHandle<AssetPackage>& GetParentPackage() const
    {
        return m_parentPackage;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE const AssetPackageSet& GetSubpackages() const
    {
        return m_subpackages;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE void SetSubpackages(const AssetPackageSet& subpackages)
    {
        m_subpackages = subpackages;
    }

    template <class Callback>
    void ForEachSubpackage(Callback&& callback) const
    {
        AssetPackageSet set;

        {
            Mutex::Guard guard(m_mutex);
            set = m_subpackages;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    HYP_FORCE_INLINE const AssetObjectSet& GetAssetObjects() const
    {
        return m_assetObjects;
    }

    void SetAssetObjects(const AssetObjectSet& assetObjects);

    template <class Callback>
    void ForEachAssetObject(Callback&& callback) const
    {
        AssetObjectSet set;

        {
            Mutex::Guard guard(m_mutex);
            set = m_assetObjects;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    template <class T>
    TResult<Handle<AssetObject>> NewAssetObject(Name name, T&& data)
    {
        Handle<AssetObject> assetObject = CreateObject<AssetObject>(name, std::forward<T>(data));
        Result result = AddAssetObject(assetObject);

        if (result.HasError())
        {
            return result.GetError();
        }

        return assetObject;
    }

    Result AddAssetObject(const Handle<AssetObject>& assetObject);
    Result RemoveAssetObject(const Handle<AssetObject>& assetObject);
    
    Handle<AssetObject> GetAssetObject(WeakName assetName) const;

    /*! \brief Merges the contents of another package into this one.
    *  Transfers ownership of all asset objects and subpackages from the source package
    *  to this package. Assets are renamed if conflicts occur, and subpackages are merged recursively.
    *  After successful merge, the source package will be empty.
    *  \param package The package to merge into this one.
    *  \return Result indicating success or failure of the merge operation. */
    Result MergePackage(const Handle<AssetPackage>& package);

    HYP_METHOD()
    String BuildPackagePath() const;

    HYP_METHOD()
    AssetPath BuildAssetPath(Name assetName) const;

    HYP_METHOD()
    bool HasAssetWithName(Name assetName) const;

    HYP_METHOD()
    Name GetUniqueAssetName(Name baseName) const;

    HYP_METHOD()
    Result Save(const FilePath& outputDirectory);

    Result OpenAssetReadStream(Name assetName, BufferedReader& stream) const;

    Delegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectAdded;
    Delegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectRemoved;

    Delegate<void, Handle<AssetPackage>> OnSubpackageAdded;
    Delegate<void, Handle<AssetPackage>> OnSubpackageRemoved;

private:
    void Init() override;

    Result SaveManifest(ByteWriter& stream) const;

    Name GetUniqueAssetName_Internal(Name baseName) const;

    HYP_FIELD(Serialize = true)
    Uuid m_uuid;

    HYP_FIELD(Serialize = true)
    Name m_name;

    HYP_FIELD(Serialize = true)
    Name m_friendlyName;

    HYP_FIELD(Serialize = true)
    EnumFlags<AssetPackageFlags> m_flags;

    WeakHandle<AssetRegistry> m_registry;
    WeakHandle<AssetPackage> m_parentPackage;
    AssetPackageSet m_subpackages;
    AssetObjectSet m_assetObjects;
    FilePath m_packageDir;
    mutable Mutex m_mutex;
};

enum class AssetRegistryPathType : uint8
{
    PACKAGE = 0,
    ASSET = 1
};

HYP_CLASS()
class HYP_API AssetRegistry final : public HypObjectBase
{
    HYP_OBJECT_BODY(AssetRegistry);

public:
    AssetRegistry();
    AssetRegistry(const String& rootPath);
    AssetRegistry(const AssetRegistry& other) = delete;
    AssetRegistry& operator=(const AssetRegistry& other) = delete;
    AssetRegistry(AssetRegistry&& other) noexcept = delete;
    AssetRegistry& operator=(AssetRegistry&& other) noexcept = delete;
    ~AssetRegistry();

    HYP_METHOD()
    HYP_FORCE_INLINE String GetRootPath() const
    {
        Mutex::Guard guard(m_mutex);
        return m_rootPath;
    }

    HYP_METHOD()
    void SetRootPath(const String& rootPath);

    HYP_FORCE_INLINE const AssetPackageSet& GetPackages() const
    {
        return m_packages;
    }

    /*! \internal Serialization only */
    void SetPackages(const AssetPackageSet& packages);

    /*! \brief Adds a package to the registry. If a package with the same name already exists and `mergeIfExists` is false,
     *  this will fail and return error.
     *  If `mergeIfExists` is true, the contents of the given package will be merged into the existing package.
     *  \param package The package to add. This reference will be updated to point to the package in the registry.
     *  \param mergeIfExists If true, and a package with the same name already exists, the contents of the given package will be merged into the existing package.
     *  \return Result indicating success or failure of the operation. */
    Result AddPackage(Handle<AssetPackage>& package, bool mergeIfExists = false);

    template <class Callback>
    void ForEachPackage(Callback&& callback) const
    {
        ForEach(m_packages, m_mutex, std::forward<Callback>(callback));
    }

    HYP_METHOD()
    Handle<AssetPackage> GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist = true);

    HYP_METHOD()
    Handle<AssetPackage> GetSubpackage(const Handle<AssetPackage>& parentPackage, Name subpackageName, bool createIfNotExist = true);

    HYP_METHOD()
    bool RemovePackage(AssetPackage* package);

    Result LoadPackageFromManifest(
        const FilePath& manifestPath,
        const String& basePackagePath,
        Handle<AssetPackage>& outPackage,
        bool loadSubpackages);

    HYP_METHOD()
    Name GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const;

    Result RegisterAsset(const UTF8StringView& path, const Handle<AssetObject>& assetObject);
    
    template <class T>
    Handle<AssetObject> NewAssetObject(const UTF8StringView& path, T&& data)
    {
        String pathString = path;
        Array<String> pathStringSplit = pathString.Split('/', '\\');

        String assetName;

        pathString = String::Join(pathStringSplit, '/');

        Mutex::Guard guard1(m_mutex);
        Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(pathString, AssetRegistryPathType::PACKAGE, /* createIfNotExist */ true, assetName);

        Handle<AssetObject> assetObject = CreateObject<AssetObject>(CreateNameFromDynamicString(assetName), std::forward<T>(data));

        assetPackage->AddAssetObject(assetObject);

        return assetObject;
    }

    Handle<AssetObject> GetAssetFromPath(const UTF8StringView& path) const;

    Delegate<void, Handle<AssetPackage>> OnPackageAdded;
    Delegate<void, Handle<AssetPackage>> OnPackageRemoved;

private:
    void Init() override;

    void LoadPackagesAsync();

    Handle<AssetPackage> GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName);

    HYP_FIELD(Serialize = true)
    String m_rootPath;

    AssetPackageSet m_packages;
    mutable Mutex m_mutex;
};

} // namespace hyperion

